using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media.Animation;
using System.Xml;

namespace EllinMS_Launcher
{
    public partial class MainWindow : Window
    {
        // ─────────────────────────────────────────────
        //  CONFIG  — edit these to match your setup
        // ─────────────────────────────────────────────
        private const string MANIFEST_URL = "https://cdn.ellinms.com/downloads.xml";
        private const string EXE_NAME     = "MapleStory.exe";
        private const string DLL_NAME     = "kaentake.dll";
        private const string USER_AGENT   = "EllinMS-Launcher/1.0";
        private const int    MAX_RETRIES  = 3;
        // ─────────────────────────────────────────────

        // Always resolves to the folder containing EllinMS Launcher.exe
        private static string GameDir =>
            Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location);

        private bool _busy = false;
        private CancellationTokenSource _cts;

        // Shared HttpClient (reused for all requests — best practice)
        private static readonly HttpClient _httpClient = CreateHttpClient();

        // ── Extensions and folders the anti-cheat should NEVER delete ────────
        private static readonly HashSet<string> SafeExtensions = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
        {
            ".wz", ".jpg", ".jpeg", ".png", ".gif", ".bmp",
            ".txt", ".log", ".ini", ".cfg", ".xml", ".json",
            ".pdb", ".config", ".manifest"
        };

        private static readonly HashSet<string> SafeFolders = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
        {
            ".vs", "bin", "obj", "logs", "screenshots", "replays"
        };

        // ─────────────────────────────────────────────────────────────────────

        public MainWindow()
        {
            // Force TLS 1.2 for Cloudflare CDN
            ServicePointManager.SecurityProtocol = (SecurityProtocolType)3072;
            InitializeComponent();
        }

        // ── Fade-in animation on load ────────────────────────────────────────

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            // Smooth fade-in + slight scale animation
            var fadeIn = new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(400))
            {
                EasingFunction = new CubicEase { EasingMode = EasingMode.EaseOut }
            };
            this.BeginAnimation(OpacityProperty, fadeIn);
        }

        // ── UI events ────────────────────────────────────────────────────────

        private void Window_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
            => DragMove();

        private void BtnClose_Click(object sender, RoutedEventArgs e)
        {
            if (_busy)
            {
                var r = MessageBox.Show("Hay una descarga en progreso. ¿Cerrar de todas formas?",
                    "EllinMS", MessageBoxButton.YesNo, MessageBoxImage.Warning);
                if (r == MessageBoxResult.No) return;
                _cts?.Cancel();
            }
            Application.Current.Shutdown();
        }

        private async void BtnPlay_Click(object sender, RoutedEventArgs e)
        {
            // If busy, treat as CANCEL button
            if (_busy)
            {
                _cts?.Cancel();
                SetStatus("Descarga cancelada.", 0);
                lblSpeed.Text = "";
                lblFileCount.Text = "";
                SetBusy(false);
                return;
            }
            await RunLauncher();
        }

        // ── Main launcher flow ───────────────────────────────────────────────

        private async Task RunLauncher()
        {
            _cts = new CancellationTokenSource();
            SetBusy(true);

            // ── Phase 1: Downloading manifest ────────────────────────────────
            SetStatus("Conectando al servidor de actualizaciones...", 0);
            SetIndeterminate(true);

            List<FileEntry> toDownload = null;
            HashSet<string> validFiles = null;
            try
            {
                // Progress callback: marshals automatically to the UI thread
                var progress = new Progress<(string status, string count)>(report =>
                {
                    lblStatus.Text    = report.status;
                    lblFileCount.Text = report.count;
                });

                var result = await Task.Run(() => CheckUpdates(progress, _cts.Token), _cts.Token);
                toDownload = result.Item1;
                validFiles = result.Item2;
            }
            catch (OperationCanceledException)
            {
                SetIndeterminate(false);
                SetStatus("Actualización cancelada.", 0);
                lblFileCount.Text = "";
                SetBusy(false);
                return;
            }
            catch (Exception ex)
            {
                SetIndeterminate(false);
                MessageBox.Show("No se pudo conectar al servidor:\n" + ex.Message,
                    "EllinMS", MessageBoxButton.OK, MessageBoxImage.Warning);
                SetStatus("Error de conexión. Revisa tu internet.", 0);
                lblFileCount.Text = "";
                SetBusy(false);
                return;
            }

            SetIndeterminate(false);

            // ── Phase 2: Download missing / changed files ─────────────────────
            if (toDownload.Count == 0)
            {
                SaveCache();
                SetStatus("¡Todos los archivos están actualizados!", 100);
                lblFileCount.Text = "";
            }
            else
            {
                lblFileCount.Text = string.Format("0 / {0} archivos", toDownload.Count);
                bool ok = await DownloadFiles(toDownload, _cts.Token);
                SaveCache();
                if (!ok)
                {
                    SetBusy(false);
                    return;
                }
                SetStatus("¡Descarga completa!", 100);
                lblFileCount.Text = "";
            }

            // ── Phase 2.5: Anti-Cheat Integrity Check ────────────────────────
            SetStatus("Verificando integridad del juego...", 100);
            int deletedCount = 0;
            await Task.Run(() =>
            {
                string launcherPath = Process.GetCurrentProcess().MainModule.FileName;
                string exePath = Path.Combine(GameDir, EXE_NAME);
                string xmlPath = Path.Combine(GameDir, "downloads.xml");

                validFiles.Add(launcherPath);
                validFiles.Add(exePath);
                validFiles.Add(xmlPath);

                // Safe exceptions — DLLs used by the game
                validFiles.Add(Path.Combine(GameDir, "ijji15.dll"));
                validFiles.Add(Path.Combine(GameDir, "ijl15.dll"));
                validFiles.Add(Path.Combine(GameDir, "kaentake.dll"));
                validFiles.Add(Path.Combine(GameDir, "gr2G_DX8"));

                string[] allFiles;
                try { allFiles = Directory.GetFiles(GameDir, "*.*", SearchOption.AllDirectories); }
                catch { return; }

                foreach (string file in allFiles)
                {
                    if (validFiles.Contains(file))
                        continue;

                    // Skip safe extensions (config, logs, images, etc.)
                    string extension = Path.GetExtension(file);
                    if (SafeExtensions.Contains(extension))
                        continue;

                    // Skip files inside safe folders
                    string relativePath = file.Substring(GameDir.Length + 1);
                    bool inSafeFolder = false;
                    foreach (var safeFolder in SafeFolders)
                    {
                        if (relativePath.StartsWith(safeFolder + "\\", StringComparison.OrdinalIgnoreCase) ||
                            relativePath.StartsWith(safeFolder + "/", StringComparison.OrdinalIgnoreCase))
                        {
                            inSafeFolder = true;
                            break;
                        }
                    }
                    if (inSafeFolder) continue;

                    try
                    {
                        File.Delete(file);
                        Interlocked.Increment(ref deletedCount);
                    }
                    catch { /* File in use or permission denied — skip silently */ }
                }
            });

            if (deletedCount > 0)
            {
                SetStatus(string.Format("Integridad verificada. {0} archivo(s) no válido(s) eliminado(s).", deletedCount), 100);
                await Task.Delay(800);
            }

            // ── Phase 3: Launch game ──────────────────────────────────────────
            SetStatus("Iniciando EllinMS...", 100);
            lblSpeed.Text = "";
            await Task.Delay(400);
            LaunchGame();
        }

        // ── Cache System ─────────────────────────────────────────────────────

        private struct CacheEntry
        {
            public long Size;
            public long Ticks;
            public string Hash;
        }

        private static System.Collections.Concurrent.ConcurrentDictionary<string, CacheEntry> _fileCache = 
            new System.Collections.Concurrent.ConcurrentDictionary<string, CacheEntry>(StringComparer.OrdinalIgnoreCase);
        
        private static string CachePath => Path.Combine(GameDir, "gr2G_DX8");
        private const string CACHE_SALT = "E1L2L3I4N5M6S7_S3CR3T_K3Y";

        private void LoadCache()
        {
            _fileCache.Clear();
            if (File.Exists(CachePath))
            {
                try
                {
                    var lines = File.ReadAllLines(CachePath);
                    if (lines.Length == 0) return;

                    string lastLine = lines[lines.Length - 1];
                    if (!lastLine.StartsWith("CHECKSUM|")) return;

                    string fileChecksum = lastLine.Substring(9);
                    var dataLines = new string[lines.Length - 1];
                    Array.Copy(lines, dataLines, lines.Length - 1);
                    
                    string allData = string.Join("\n", dataLines) + CACHE_SALT;
                    using (var md5 = System.Security.Cryptography.MD5.Create())
                    {
                        string computed = BitConverter.ToString(md5.ComputeHash(System.Text.Encoding.UTF8.GetBytes(allData))).Replace("-", "").ToLowerInvariant();
                        if (computed != fileChecksum) return; // Tampered! Cache invalidated.
                    }

                    foreach (var line in dataLines)
                    {
                        var parts = line.Split('|');
                        if (parts.Length == 4)
                        {
                            if (long.TryParse(parts[1], out long size) && long.TryParse(parts[2], out long ticks))
                            {
                                _fileCache[parts[0]] = new CacheEntry { Size = size, Ticks = ticks, Hash = parts[3] };
                            }
                        }
                    }
                }
                catch { }
            }
        }

        private void SaveCache()
        {
            try
            {
                var lines = new List<string>(_fileCache.Count + 1);
                foreach (var kvp in _fileCache)
                {
                    lines.Add(string.Format("{0}|{1}|{2}|{3}", kvp.Key, kvp.Value.Size, kvp.Value.Ticks, kvp.Value.Hash));
                }
                
                string allData = string.Join("\n", lines) + CACHE_SALT;
                using (var md5 = System.Security.Cryptography.MD5.Create())
                {
                    string checksum = BitConverter.ToString(md5.ComputeHash(System.Text.Encoding.UTF8.GetBytes(allData))).Replace("-", "").ToLowerInvariant();
                    lines.Add("CHECKSUM|" + checksum);
                }

                File.WriteAllLines(CachePath, lines);
                
                // Hide the file to confuse players
                try
                {
                    var attributes = File.GetAttributes(CachePath);
                    if ((attributes & FileAttributes.Hidden) != FileAttributes.Hidden)
                    {
                        File.SetAttributes(CachePath, attributes | FileAttributes.Hidden);
                    }
                }
                catch { }
            }
            catch { }
        }

        // ── Update check (runs on background thread) ─────────────────────────

        private struct FileEntry
        {
            public string Name;   // relative path, forward slashes
            public string Hash;   // md5 lowercase hex
            public string Url;
            public long   Size;
        }

        private Tuple<List<FileEntry>, HashSet<string>> CheckUpdates(IProgress<(string status, string count)> progress, CancellationToken ct)
        {
            LoadCache();

            // ── Step 1: Fetch manifest ─────────────────────────────────────
            progress.Report(("Conectando al servidor de actualizaciones...", ""));

            ct.ThrowIfCancellationRequested();

            string xml;
            using (var wc = MakeWebClient())
                xml = wc.DownloadString(MANIFEST_URL);

            progress.Report(("Leyendo lista de archivos...", ""));

            var doc = new XmlDocument();
            doc.LoadXml(xml);

            var nodes   = doc.SelectNodes("//file");
            int total   = nodes.Count;
            var dir     = GameDir;

            // Build list of all entries from manifest
            var allEntries = new List<FileEntry>(total);
            var validFiles = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (XmlNode node in nodes)
            {
                var name = node["file_name"]?.InnerText;
                var hash = node["file_hash"]?.InnerText;
                var url  = node["file_link"]?.InnerText;
                long.TryParse(node["file_size"]?.InnerText ?? "0", out long size);
                if (!string.IsNullOrEmpty(name) && !string.IsNullOrEmpty(url))
                {
                    allEntries.Add(new FileEntry { Name = name, Hash = hash, Url = url, Size = size });
                    var localPath = Path.Combine(dir, name.Replace('/', '\\'));
                    validFiles.Add(localPath);
                }
            }

            // ── Step 2: Parallel hash verification ──────────────────────────
            var toDownload = new System.Collections.Concurrent.ConcurrentBag<FileEntry>();
            int verified   = 0;

            var options = new ParallelOptions
            {
                MaxDegreeOfParallelism = Environment.ProcessorCount,
                CancellationToken = ct
            };

            var launcherPath = Process.GetCurrentProcess().MainModule.FileName;

            Parallel.ForEach(allEntries, options, entry =>
            {
                ct.ThrowIfCancellationRequested();

                var localPath = Path.Combine(dir, entry.Name.Replace('/', '\\'));
                bool needsDownload = false;

                // NUNCA intentar actualizar o borrar el propio Launcher mientras está corriendo
                if (localPath.Equals(launcherPath, StringComparison.OrdinalIgnoreCase))
                {
                    needsDownload = false;
                }
                else if (!File.Exists(localPath))
                {
                    needsDownload = true;
                }
                else
                {
                    var info = new FileInfo(localPath);

                    if (info.Length != entry.Size)
                    {
                        needsDownload = true;
                    }
                    else
                    {
                        long ticks = info.LastWriteTimeUtc.Ticks;
                        
                        // Intelligent Cache Check
                        if (_fileCache.TryGetValue(entry.Name, out var cached) && cached.Size == info.Length && cached.Ticks == ticks)
                        {
                            needsDownload = cached.Hash != entry.Hash;
                        }
                        else
                        {
                            // Cache miss or changed file - recalculate MD5
                            string computedHash = ComputeMd5(localPath);
                            needsDownload = computedHash != entry.Hash;
                            
                            // Update cache with the newly calculated hash for next time
                            if (!needsDownload)
                            {
                                _fileCache[entry.Name] = new CacheEntry { Size = info.Length, Ticks = ticks, Hash = computedHash };
                            }
                        }
                    }
                }

                if (needsDownload)
                    toDownload.Add(entry);

                int done = Interlocked.Increment(ref verified);

                // Solo reportar progreso cada 100 archivos para no saturar la UI
                if (done % 100 == 0 || done == allEntries.Count)
                {
                    progress.Report((
                        string.Format("Verificando: {0}", entry.Name),
                        string.Format("Verificados {0} / {1}", done, allEntries.Count)
                    ));
                }
            });

            SaveCache();

            return new Tuple<List<FileEntry>, HashSet<string>>(new List<FileEntry>(toDownload), validFiles);
        }

        // ── Async download with HttpClient, speed tracking, auto-retry ───────

        private async Task<bool> DownloadFiles(IEnumerable<FileEntry> fileList, CancellationToken ct)
        {
            var files = new List<FileEntry>(fileList);
            var dir = GameDir;
            int done = 0;
            bool anyFailed = false;

            var semaphore = new System.Threading.SemaphoreSlim(10); // 10 parallel downloads max
            var downloadTasks = new List<Task>();

            foreach (var f in files)
            {
                if (ct.IsCancellationRequested) return false;

                downloadTasks.Add(Task.Run(async () =>
                {
                    await semaphore.WaitAsync(ct);
                    try
                    {
                        if (ct.IsCancellationRequested || anyFailed) return;

                        var localPath = Path.Combine(dir, f.Name.Replace('/', '\\'));
                        var localDir = Path.GetDirectoryName(localPath);
                        if (!string.IsNullOrEmpty(localDir) && !Directory.Exists(localDir))
                            Directory.CreateDirectory(localDir);

                        if (File.Exists(localPath))
                        {
                            try { File.Delete(localPath); } catch { }
                        }

                        bool success = false;
                        int attempt = 0;

                        while (!success && attempt < MAX_RETRIES)
                        {
                            attempt++;
                            try
                            {
                                await DownloadFileWithProgress(f, localPath, ct);
                                success = true;
                            }
                            catch (OperationCanceledException)
                            {
                                return;
                            }
                            catch (Exception)
                            {
                                if (attempt >= MAX_RETRIES)
                                {
                                    anyFailed = true;
                                    return;
                                }
                                await Task.Delay(1000 * attempt, ct);
                            }
                        }

                        if (success)
                        {
                            try
                            {
                                var fi = new FileInfo(localPath);
                                _fileCache[f.Name] = new CacheEntry { Size = fi.Length, Ticks = fi.LastWriteTimeUtc.Ticks, Hash = f.Hash };
                            }
                            catch { }

                            int d = Interlocked.Increment(ref done);
                            Dispatcher.InvokeAsync(() =>
                            {
                                if (!ct.IsCancellationRequested)
                                    lblFileCount.Text = string.Format("{0} / {1} archivos", d, files.Count);
                            });
                        }
                    }
                    finally
                    {
                        semaphore.Release();
                    }
                }));
            }

            try
            {
                await Task.WhenAll(downloadTasks);
            }
            catch (OperationCanceledException)
            {
                SetStatus("Descarga cancelada.", 0);
                return false;
            }

            if (anyFailed)
            {
                MessageBox.Show("Hubo un error descargando algunos archivos. Por favor, reintenta.", "EllinMS", MessageBoxButton.OK, MessageBoxImage.Error);
                return false;
            }

            SaveCache();
            lblFileCount.Text = string.Format("{0} / {0} archivos", files.Count);
            return true;
        }

        /// <summary>
        /// Downloads a single file using HttpClient with real-time speed and progress tracking.
        /// </summary>
        private async Task DownloadFileWithProgress(FileEntry f, string localPath, CancellationToken ct)
        {
            using (var response = await _httpClient.GetAsync(f.Url, HttpCompletionOption.ResponseHeadersRead, ct))
            {
                response.EnsureSuccessStatusCode();
                long totalBytes = response.Content.Headers.ContentLength ?? f.Size;

                using (var contentStream = await response.Content.ReadAsStreamAsync())
                using (var fileStream = new FileStream(localPath, FileMode.Create, FileAccess.Write, FileShare.None, 81920, true))
                {
                    var buffer = new byte[81920];
                    long downloaded = 0;
                    int bytesRead;
                    var sw = Stopwatch.StartNew();
                    long lastSpeedUpdate = 0;

                    while ((bytesRead = await contentStream.ReadAsync(buffer, 0, buffer.Length, ct)) > 0)
                    {
                        await fileStream.WriteAsync(buffer, 0, bytesRead, ct);
                        downloaded += bytesRead;

                        // Update progress on UI thread (throttled to ~10 updates/sec)
                        long elapsed = sw.ElapsedMilliseconds;
                        if (elapsed - lastSpeedUpdate >= 100 || downloaded == totalBytes)
                        {
                            lastSpeedUpdate = elapsed;
                            double seconds = elapsed / 1000.0;
                            double speedMBps = seconds > 0 ? (downloaded / 1048576.0) / seconds : 0;
                            int percent = totalBytes > 0 ? (int)(downloaded * 100 / totalBytes) : 0;

                            // Marshal to UI thread
                            Dispatcher.Invoke(() =>
                            {
                                pbProgress.Value = percent;
                                if (totalBytes > 0)
                                    lblSpeed.Text = string.Format("{0:0.00} MB / {1:0.00} MB  •  {2:0.00} MB/s",
                                        downloaded / 1048576.0,
                                        totalBytes / 1048576.0,
                                        speedMBps);
                            });
                        }
                    }
                }
            }
        }

        // ── Launch MapleStory.exe ───────────────────────────────────────

        private void LaunchGame()
        {
            var exePath = Path.Combine(GameDir, EXE_NAME);

            if (!File.Exists(exePath))
            {
                MessageBox.Show("No se encontró " + EXE_NAME + " en la carpeta del juego.",
                    "EllinMS", MessageBoxButton.OK, MessageBoxImage.Error);
                SetBusy(false);
                return;
            }

            try
            {
                // Set environment variable so injected DLL knows we launched it
                Environment.SetEnvironmentVariable("ELLINMS_LAUNCHER", "1");

                var startInfo = new ProcessStartInfo();
                startInfo.FileName = exePath;
                startInfo.WorkingDirectory = GameDir;
                startInfo.UseShellExecute = false;

                var proc = Process.Start(startInfo);

                if (proc == null || proc.HasExited)
                {
                    MessageBox.Show("El proceso del juego no pudo iniciarse.",
                        "EllinMS", MessageBoxButton.OK, MessageBoxImage.Error);
                    SetBusy(false);
                    return;
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show("Error al iniciar el juego:\n" + ex.Message,
                    "EllinMS", MessageBoxButton.OK, MessageBoxImage.Error);
                SetBusy(false);
                return;
            }

            Application.Current.Shutdown();
        }

        // ── Helpers ───────────────────────────────────────────────────────────

        private static HttpClient CreateHttpClient()
        {
            var handler = new HttpClientHandler
            {
                AutomaticDecompression = DecompressionMethods.GZip | DecompressionMethods.Deflate
            };
            var client = new HttpClient(handler);
            client.DefaultRequestHeaders.UserAgent.ParseAdd(USER_AGENT);
            client.Timeout = TimeSpan.FromMinutes(10);
            return client;
        }

        // WebClient is still used for the small manifest download (simple & sync-friendly)
        private WebClient MakeWebClient()
        {
            var wc = new WebClient();
            wc.Headers.Add("User-Agent", USER_AGENT);
            return wc;
        }

        private static string ComputeMd5(string path)
        {
            const int BUFFER = 1024 * 1024;
            using (var fs  = new FileStream(path, FileMode.Open, FileAccess.Read,
                                FileShare.Read, BUFFER, FileOptions.SequentialScan))
            using (var md5 = MD5.Create())
            {
                return BitConverter.ToString(md5.ComputeHash(fs))
                                   .Replace("-", "")
                                   .ToLowerInvariant();
            }
        }

        private void SetBusy(bool busy)
        {
            _busy             = busy;
            btnPlay.IsEnabled = true; // Always enabled — toggles between PLAY and CANCEL

            // Toggle button text
            Dispatcher.Invoke(() =>
            {
                btnPlay.Content = busy ? "CANCEL" : "PLAY";
            });
        }

        private void SetIndeterminate(bool on)
        {
            pbProgress.IsIndeterminate = on;
            if (!on) pbProgress.Value = 0;
        }

        private void SetStatus(string text, double progress)
        {
            lblStatus.Text   = text;
            if (!pbProgress.IsIndeterminate)
                pbProgress.Value = progress;
        }
    }
}
