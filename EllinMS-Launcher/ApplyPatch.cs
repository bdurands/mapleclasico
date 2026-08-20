using System;
using System.IO;

class Program {
    static void Main() {
        string path = @"C:\Users\Nion\Desktop\mapleclasico\EllinMS-Launcher\EllinMS-Launcher\MainWindow.xaml.cs";
        string content = File.ReadAllText(path);

        // 1. Add validFiles.Add("launcher_cache.dat")
        string target1 = "validFiles.Add(Path.Combine(GameDir, "kaentake.dll"));";
        string replace1 = target1 + "\n                validFiles.Add(Path.Combine(GameDir, "launcher_cache.dat"));";
        content = content.Replace(target1, replace1);

        // 2. Add SaveCache() when toDownload.Count == 0
        string target2 = @"            if (toDownload.Count == 0)
            {
                SetStatus("¡Todos los archivos están actualizados!", 100);";
        string replace2 = @"            if (toDownload.Count == 0)
            {
                SaveCache();
                SetStatus("¡Todos los archivos están actualizados!", 100);";
        content = content.Replace(target2, replace2);

        // 3. Add SaveCache() after DownloadFiles
        string target3 = @"                bool ok = await DownloadFiles(toDownload, _cts.Token);
                if (!ok)";
        string replace3 = @"                bool ok = await DownloadFiles(toDownload, _cts.Token);
                SaveCache();
                if (!ok)";
        content = content.Replace(target3, replace3);

        // 4. Inject Cache System before CheckUpdates
        string target4 = @"        // -- Update check (runs on background thread) -------------------------

        private struct FileEntry";
        string replace4 = @"        // -- Cache System -----------------------------------------------------

        private struct CacheEntry
        {
            public long Size;
            public long Ticks;
            public string Hash;
        }

        private static System.Collections.Concurrent.ConcurrentDictionary<string, CacheEntry> _fileCache = 
            new System.Collections.Concurrent.ConcurrentDictionary<string, CacheEntry>(StringComparer.OrdinalIgnoreCase);
        
        private static string CachePath => Path.Combine(GameDir, "launcher_cache.dat");
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
                var lines = new System.Collections.Generic.List<string>(_fileCache.Count + 1);
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
            }
            catch { }
        }

" + target4;
        content = content.Replace(target4, replace4);

        // 5. Add LoadCache() to CheckUpdates
        string target5 = @"        private Tuple<List<FileEntry>, HashSet<string>> CheckUpdates(IProgress<(string status, string count)> progress, CancellationToken ct)
        {
            // -- Step 1: Fetch manifest -------------------------------------";
        string replace5 = @"        private Tuple<List<FileEntry>, HashSet<string>> CheckUpdates(IProgress<(string status, string count)> progress, CancellationToken ct)
        {
            LoadCache();

            // -- Step 1: Fetch manifest -------------------------------------";
        content = content.Replace(target5, replace5);

        // 6. Update Cache Check Logic in CheckUpdates
        string target6 = @"                    if (info.Length != entry.Size)
                    {
                        needsDownload = true;
                    }
                    else
                    {
                        needsDownload = ComputeMd5(localPath) != entry.Hash;
                    }";
        string replace6 = @"                    if (info.Length != entry.Size)
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
                    }";
        content = content.Replace(target6, replace6);

        // 7. Call SaveCache() at end of CheckUpdates
        string target7 = @"            return new Tuple<List<FileEntry>, HashSet<string>>(new List<FileEntry>(toDownload), validFiles);
        }";
        string replace7 = @"            SaveCache();
            return new Tuple<List<FileEntry>, HashSet<string>>(new List<FileEntry>(toDownload), validFiles);
        }";
        // Be careful with Replace here, there are multiple returns, but the last one is the only one like this.
        content = content.Replace(@"            return new Tuple<List<FileEntry>, HashSet<string>>(new List<FileEntry>(toDownload), validFiles);", @"            SaveCache();
            return new Tuple<List<FileEntry>, HashSet<string>>(new List<FileEntry>(toDownload), validFiles);");


        // 8. Update cache immediately after download
        string target8 = @"                if (success)
                {
                    done++;
                    pbProgress.Value = 0;
                    lblSpeed.Text = "";
                }";
        string replace8 = @"                if (success)
                {
                    // Update cache immediately after download
                    try
                    {
                        var fi = new FileInfo(localPath);
                        _fileCache[f.Name] = new CacheEntry { Size = fi.Length, Ticks = fi.LastWriteTimeUtc.Ticks, Hash = f.Hash };
                    }
                    catch { }

                    done++;
                    pbProgress.Value = 0;
                    lblSpeed.Text = "";
                }";
        content = content.Replace(target8, replace8);

        File.WriteAllText(path, content);
    }
}
