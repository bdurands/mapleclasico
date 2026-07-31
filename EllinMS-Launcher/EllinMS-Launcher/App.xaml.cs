using System;
using System.Threading;
using System.Windows;

namespace EllinMS_Launcher
{
    public partial class App : Application
    {
        private static Mutex _mutex = null;

        protected override void OnStartup(StartupEventArgs e)
        {
            const string appName = "EllinMS_Launcher_SingleInstance_Mutex";
            bool createdNew;

            _mutex = new Mutex(true, appName, out createdNew);

            if (!createdNew)
            {
                MessageBox.Show("El launcher ya se encuentra en ejecución.", "EllinMS Launcher", MessageBoxButton.OK, MessageBoxImage.Information);
                Application.Current.Shutdown();
                return;
            }

            base.OnStartup(e);

            // Atrapa cualquier error grave que cierre el Launcher silenciosamente
            this.DispatcherUnhandledException += (s, args) =>
            {
                MessageBox.Show("CRASH DETECTADO:\n" + args.Exception.ToString(), "Error Crítico", MessageBoxButton.OK, MessageBoxImage.Error);
                args.Handled = true;
            };

            AppDomain.CurrentDomain.UnhandledException += (s, args) =>
            {
                MessageBox.Show("CRASH FATAL:\n" + args.ExceptionObject.ToString(), "Error Crítico", MessageBoxButton.OK, MessageBoxImage.Error);
            };
        }

        protected override void OnExit(ExitEventArgs e)
        {
            if (_mutex != null)
            {
                _mutex.ReleaseMutex();
                _mutex.Close();
            }
            base.OnExit(e);
        }
    }
}
