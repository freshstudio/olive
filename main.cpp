/***

  Olive - Non-Linear Video Editor
  Copyright (C) 2019  Olive Team

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

***/

#include <QApplication>
#include <QMessageBox>

#ifdef __GNUC__
#ifdef Q_OS_WIN
#include <windows.h>
#include <DbgHelp.h>
#elif defined(Q_OS_LINUX)
#include <execinfo.h>
#endif
#include <signal.h>
#include <unistd.h>
#endif

#include "dialogs/crashdialog.h"
#include "global/debug.h"
#include "global/config.h"
#include "global/global.h"
#include "panels/timeline.h"
#include "rendering/pixelformats.h"
#include "ui/mediaiconservice.h"
#include "ui/mainwindow.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
}

#ifdef __GNUC__
void handler(int sig) {
  QStringList strings;

#ifdef Q_OS_WIN
  HANDLE process = GetCurrentProcess();
  HANDLE thread = GetCurrentThread();

  CONTEXT context;
  memset(&context, 0, sizeof(CONTEXT));
  context.ContextFlags = CONTEXT_FULL;
  RtlCaptureContext(&context);

  SymInitialize(process, NULL, TRUE);

  DWORD image;
  STACKFRAME64 stackframe;
  ZeroMemory(&stackframe, sizeof(STACKFRAME64));

#ifdef _M_IX86
  image = IMAGE_FILE_MACHINE_I386;
  stackframe.AddrPC.Offset = context.Eip;
  stackframe.AddrPC.Mode = AddrModeFlat;
  stackframe.AddrFrame.Offset = context.Ebp;
  stackframe.AddrFrame.Mode = AddrModeFlat;
  stackframe.AddrStack.Offset = context.Esp;
  stackframe.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
  image = IMAGE_FILE_MACHINE_AMD64;
  stackframe.AddrPC.Offset = context.Rip;
  stackframe.AddrPC.Mode = AddrModeFlat;
  stackframe.AddrFrame.Offset = context.Rsp;
  stackframe.AddrFrame.Mode = AddrModeFlat;
  stackframe.AddrStack.Offset = context.Rsp;
  stackframe.AddrStack.Mode = AddrModeFlat;
#elif _M_IA64
  image = IMAGE_FILE_MACHINE_IA64;
  stackframe.AddrPC.Offset = context.StIIP;
  stackframe.AddrPC.Mode = AddrModeFlat;
  stackframe.AddrFrame.Offset = context.IntSp;
  stackframe.AddrFrame.Mode = AddrModeFlat;
  stackframe.AddrBStore.Offset = context.RsBSP;
  stackframe.AddrBStore.Mode = AddrModeFlat;
  stackframe.AddrStack.Offset = context.IntSp;
  stackframe.AddrStack.Mode = AddrModeFlat;
#endif

  int counter = 0;
  QString line_template = "[%1] %2";
  while (StackWalk64(
           image, process, thread,
           &stackframe, &context, NULL,
           SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {

    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

    DWORD64 displacement = 0;


    QString sym_name;
    if (SymFromAddr(process, stackframe.AddrPC.Offset, &displacement, symbol)) {
      sym_name = symbol->Name;
    } else {
      sym_name = "???";
    }
    QString s = line_template.arg(QString::number(counter), sym_name);
    strings.append(s);
    qDebug() << s;

    counter++;
  }

  SymCleanup(process);
#elif defined(Q_OS_LINUX)
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Signal: %d\n\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);

  // try to show a GUI crash report
  char** bt_syms = backtrace_symbols(array, size);
  for (int i=0;i<size;i++) {
    strings.append(bt_syms[i]);
  }
  free(bt_syms);

#endif

  olive::crash_dialog->SetData(sig, strings);
  olive::crash_dialog->exec();

  abort();
}
#endif

int main(int argc, char *argv[]) {
#ifdef __GNUC__
  signal(SIGSEGV, handler);
#endif

  olive::Global = std::unique_ptr<OliveGlobal>(new OliveGlobal);

  bool launch_fullscreen = false;
  QString load_proj;

  bool use_internal_logger = true;

  if (argc > 1) {
    for (int i=1;i<argc;i++) {
      if (argv[i][0] == '-') {
        if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-v")) {
#ifndef GITHASH
          qWarning() << "No Git commit information found";
#endif
          printf("%s\n", olive::AppName.toUtf8().constData());
          return 0;
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
          printf("Usage: %s [options] [filename]\n\n"
                 "[filename] is the file to open on startup.\n\n"
                 "Options:\n"
                 "\t-v, --version\t\tShow version information\n"
                 "\t-h, --help\t\tShow this help\n"
                 "\t-f, --fullscreen\tStart in full screen mode\n"
                 "\t--disable-shaders\tDisable OpenGL shaders (for debugging)\n"
                 "\t--no-debug\t\tDisable internal debug log and output directly to console\n"
                 "\t--translation <file>\tSet an external language file to use\n"
                 "\n"
                 "Environment Variables:\n"
                 "\tOLIVE_EFFECTS_PATH\tSpecify a path to search for GLSL shader effects\n"
                 "\tFREI0R_PATH\t\tSpecify a path to search for Frei0r effects\n"
                 "\tOLIVE_LANG_PATH\t\tSpecify a path to search for translation files\n"
                 "\n", argv[0]);
          return 0;
        } else if (!strcmp(argv[i], "--fullscreen") || !strcmp(argv[i], "-f")) {
          launch_fullscreen = true;
        } else if (!strcmp(argv[i], "--disable-shaders")) {
          olive::runtime_config.shaders_are_enabled = false;
        } else if (!strcmp(argv[i], "--no-debug")) {
          use_internal_logger = false;
        } else if (!strcmp(argv[i], "--translation")) {
          if (i + 1 < argc && argv[i + 1][0] != '-') {
            // load translation file
            olive::runtime_config.external_translation_file = argv[i + 1];

            i++;
          } else {
            printf("[ERROR] No translation file specified\n");
            return 1;
          }
        } else {
          printf("[ERROR] Unknown argument '%s'\n", argv[1]);
          return 1;
        }
      } else if (load_proj.isEmpty()) {
        load_proj = argv[i];
      }
    }
  }

  if (use_internal_logger) {
    qInstallMessageHandler(debug_message_handler);
  }

  // Initialize ffmpeg subsystem
  // (these have been deprecated in FFmpeg 4, but are still necessary for FFmpeg 3)
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100)
  av_register_all();
#endif

#if LIBAVFILTER_VERSION_INT < AV_VERSION_INT(7, 14, 100)
  avfilter_register_all();
#endif

  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

  QSurfaceFormat format;
  format.setVersion(3, 2);
  format.setDepthBufferSize(24);
  format.setProfile(QSurfaceFormat::CoreProfile);
  QSurfaceFormat::setDefaultFormat(format);

  QApplication a(argc, argv);
  a.setWindowIcon(QIcon(":/icons/olive64.png"));

  // start media icon service (uses QPixmaps which require a QGuiApplication to have been created)
  olive::media_icon_service = std::unique_ptr<MediaIconService>(new MediaIconService());

  // set app name data
  QCoreApplication::setOrganizationName("olivevideoeditor.org");
  QCoreApplication::setOrganizationDomain("olivevideoeditor.org");
  QCoreApplication::setApplicationName("Olive");

#if (QT_VERSION >= QT_VERSION_CHECK(5, 7, 0))
  QGuiApplication::setDesktopFileName("org.olivevideoeditor.Olive");
#endif

  olive::crash_dialog = new CrashDialog();

  MainWindow w(nullptr);

  // multiply track height constants by the current DPI scale
  olive::timeline::MultiplyTrackSizesByDPI();

  // set up rendering bit depths
  olive::InitializePixelFormats();

  // connect main window's first paint to global's init finished function
  QObject::connect(&w, SIGNAL(finished_first_paint()), olive::Global.get(), SLOT(finished_initialize()), Qt::QueuedConnection);

  if (!load_proj.isEmpty()) {
    olive::Global->load_project_on_launch(load_proj);
  }
  if (launch_fullscreen) {
    w.showFullScreen();
  } else {
    w.showMaximized();
  }

  return a.exec();
}
