#include "figmaget.h"
#include "figmaqml.h"
#include "clipboard.h"
#include "downloads.h"
#include "functorslot.h"
#include "utils.h"
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSslSocket>
#include <QMessageBox>
#include <QSurfaceFormat>
#include <QSettings>
#include <QTemporaryDir>
#include <QCommandLineParser>
#include <QTextStream>
#include <QRegularExpression>
#include <QFont>
#include <QQuickStyle>
#include "app_figma/FigmaQmlInterface/FigmaQmlInterface.hpp"



#ifndef NO_SSL
QT_REQUIRE_CONFIG(ssl);
#endif



constexpr char PROJECT_TOKEN[]{"project_token"};
constexpr char USER_TOKEN[]{"user_token"};
constexpr char FLAGS[]{"flags"};
constexpr char EMBED_IMAGES[]{"embed_images"};
constexpr char IMAGEMAXSIZE[]{"image_max_size"};
constexpr char FONTFOLDER[]{"font_folder"};
constexpr char THROTTLE[]{"throttle"};
constexpr char FONTMAP[]{"font_map"};
constexpr char IMPORTS[]{"imports"};
constexpr char COMPANY_NAME[]{"Moonshine shade of haste productions"};
constexpr char PRODUCT_NAME[]{"FigmaQML"};

constexpr auto ORGANIZATION_NAME {"FigmaQML"};
constexpr auto ORGANIZATION_DOMAIN {"figmaqml.com"};

#ifdef _DEBUG
    #define print() qDebug()
#else
QTextStream& print() {
    static QTextStream r{stdout};
    return r;
}
#endif


static QVector<QPair<QString, QString>> parseFontMap(const QString& str, bool useAlt) {
    const auto list = str.split(';');
    QVector<QPair<QString, QString>> pairs;
    for(const auto& pair : list) {
        static const QRegularExpression rex(R"(^\s*(.+)\s*\:\s*(.+)\s*$)");
        const auto m = rex.match(pair);
        if(!m.hasMatch()) {
            ::print() << "Error: Invalid font map" << pair << "in" << str << Qt::endl;
            return QVector<QPair<QString, QString>>();
        }
        const QFont font(m.captured(2));
        if(font.exactMatch()) {
            pairs.append({m.captured(1), font.family()});
        } else {
            const auto typeface = FigmaQml::nearestFontFamily(m.captured(2), useAlt);
            if(typeface != m.captured(2)) //just how it works :-)
                ::print() << QString("Warning: %1 not found, using %2 instead").arg(m.captured(2), typeface) << Qt::endl;
            pairs.append({m.captured(1), typeface});
        }
    }
   return pairs;
}

enum {
    CmdLine = 1,
    Store = 2,
    ShowFonts = 4
};





 ///main

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QCommandLineParser parser;
    const auto versionNumber = QString(STRINGIFY(VERSION_NUMBER));

    app.setOrganizationName(ORGANIZATION_NAME);
    app.setOrganizationDomain(ORGANIZATION_DOMAIN);
    app.setApplicationName(PRODUCT_NAME);

    parser.setApplicationDescription(QString("Figma to QML generator, version: %1, Markus Mertama 2020").arg(versionNumber));
    parser.addHelpOption();
    const QCommandLineOption renderFrameParameter("render-frame", "Render frames as images.");
    const QCommandLineOption imageDimensionMaxParameter("image-dimension-max", "Capping an image size, default is 1024.", "imageDimensionMax");
    const QCommandLineOption embedImagesParameter("embed-images", "Embed images into QML files.");
    const QCommandLineOption breakBooleansParameter("break-boolean", "Break Figma boolean shapes to QtQuick items.");
    const QCommandLineOption antialiasingShapesParameter("antialiasing-shapes", "Add antialiasing property to shapes.");
    const QCommandLineOption importsParameter("imports", "QML imports, ';' separated list of imported modules as <module-name> <version-number>.", "imports");
    const QCommandLineOption snapParameter("snap", "Take snapshot and exit, expects restore or user project token parameters to be given.", "snapFile");
    const QCommandLineOption storeParameter("store", "Create .figmaqml file and exit, expects user and project token parameters to be given.");
    const QCommandLineOption timedParameter("timed", "Time parsing process.");
    const QCommandLineOption figmaFontParameter("keepFigmaFont", "Do not resolve fonts, keep original font names.");
    const QCommandLineOption showFontsParameter("show-fonts", "Show the font mapping.");
    const QCommandLineOption fontFolderParameter("font-folder", "Add an additional path to search fonts.", "fontFolder");
    const QCommandLineOption showParameter("show", "Set current page and view to <page index>-<view index>, indexing starts from 1.", "show");
    const QCommandLineOption altFontMatchParameter("alt-font-match", "Use alternative font matching algorithm.");
    const QCommandLineOption fontMapParameter("font-map", "Provide a ';' separated list of <figma font>':'<system font> pairs.", "fontMap");
    const QCommandLineOption throttleParameter("throttle", "Milliseconds between server requests. Too frequent request may have issues, especially with big desings - default 300", "throttle");
    const QCommandLineOption qulmodeParameter("qul-mode", "QtQuick for Qt for MCU");
    const QCommandLineOption staticCodeParameter("static-code", "Do not generate any dynamic, interactive code, property access, event handlers etc.");

    parser.addPositionalArgument("argument 1", "Optional: .figmaqml file or user token. GUI opened if empty.", "<FIGMAQML_FILE>|<USER_TOKEN>");
    parser.addPositionalArgument("argument 2", "Optional: Output directory name (or .figmaqml file name if '--store' is given), assuming the first parameter was the restored file. If empty, GUI is opened. Project token is expected if the first parameter was an user token.", "<OUTPUT if FIGMAQML_FILE>| PROJECT_TOKEN if USER_TOKEN");
    parser.addPositionalArgument("argument 3", "Optional: Output directory name (or .figmaqml file name if '--store' is given), assuming the user and project tokens were provided. ", "<OUTPUT if USER_TOKEN>");

    parser.addOptions({
                          renderFrameParameter,
                          imageDimensionMaxParameter,
                          breakBooleansParameter,
                          antialiasingShapesParameter,
                          embedImagesParameter,
                          importsParameter,
                          snapParameter,
                          storeParameter,
                          timedParameter,
                          showParameter,
                          showFontsParameter,
                          fontFolderParameter,
                          altFontMatchParameter,
                          fontMapParameter,
                          throttleParameter,
                          figmaFontParameter,
                          staticCodeParameter,
#ifdef HAS_QUL
                          qulmodeParameter,
#endif
                      });

    parser.process(app);

    int state = 0;

    QString restore;
    QString userToken;

    if(parser.positionalArguments().size() > 0) {
        const auto name = parser.positionalArguments().at(0);
        if(name.endsWith(".figmaqml") || QFile::exists(name)) {
          if(!QFile::exists(name)) {
              ::print() << "Error:" << name << " not found" << Qt::endl;
              return -1;
          };
          restore = name;
        } else
            userToken = name;
    }

    QString projectToken;
    QString output;
    if(parser.positionalArguments().size() > 1) {
        if(!restore.isEmpty())
            output = parser.positionalArguments().at(1);
        else
            projectToken = parser.positionalArguments().at(1);
    }

    if(!userToken.isEmpty() && projectToken.isEmpty()) {
        parser.showHelp(-10);
    }

    if(!userToken.isEmpty() && parser.positionalArguments().size() > 2) {
        output = parser.positionalArguments().at(2);
    }

    const QString snapFile = parser.value(snapParameter);

    if(!output.isEmpty())
        state |= CmdLine;

    if(parser.isSet(storeParameter))
        state |= Store;

    if(parser.isSet(showFontsParameter))
        state |= ShowFonts;

    if(!snapFile.isEmpty() && !(userToken.isEmpty() || restore.isEmpty())) {
        parser.showHelp(-2);
    }

     const QString fontFolder = state & CmdLine ?
                 parser.value(fontFolderParameter) :
                 QSettings(COMPANY_NAME, PRODUCT_NAME).value(FONTFOLDER).toString();


    int canvas = 1;
    int element = 1;

    if(parser.isSet(showParameter)) {
        const auto ce = parser.value(showParameter).split('-');
        if(ce.length() != 2) {
            parser.showHelp(-11);
        }
        bool ok;
        canvas = ce[0].toInt(&ok);
        if(!ok) parser.showHelp(-12);
        element = ce[1].toInt(&ok);
        if(!ok) parser.showHelp(-13);
    }


#ifndef NO_SSL
    if (!QSslSocket::supportsSsl()) {
        const auto v = QSslSocket::sslLibraryBuildVersionString();
        QMessageBox::warning(nullptr,
                             "Secure Socket Client",
                             v.isEmpty() ?  QString("Cannot load SSL/TLS - not found") :  QString("Cannot load SSL/TLS - looking for version is %1").arg(v));
        return -1;
    }
#endif

    QSurfaceFormat format;
    format.setSamples(8);
    QSurfaceFormat::setDefaultFormat(format);

    QTemporaryDir dir;
    if(!dir.isValid()) {
        ::print() << "Cannot create temp directory (" << dir.path() << ")";
        app.exit(-12);
    }

    auto figmaGet = std::make_unique<FigmaGet>();
    auto figmaQml = std::make_unique<FigmaQml>(dir.path(), fontFolder, *figmaGet);

    Clipboard clipboard;
   // figmaQml->setFilter({{1,{2}}});

    figmaQml->setBrokenPlaceholder(":/broken_image.jpg");


    bool supressErrors = false;

    Execute onDataChange;

    if(state & CmdLine || !snapFile.isEmpty()) {
        unsigned qmlFlags = 0;

        if(restore.isEmpty()) {
            if(parser.isSet(renderFrameParameter))
                qmlFlags |= FigmaQml::PrerenderFrames;
            if(parser.isSet(breakBooleansParameter))
                qmlFlags |= FigmaQml::BreakBooleans;
            if(parser.isSet(antialiasingShapesParameter))
                qmlFlags |= FigmaQml::AntialiasingShapes;
            if(parser.isSet(embedImagesParameter))
                qmlFlags |= FigmaQml::EmbedImages;
            if(parser.isSet(altFontMatchParameter))
                qmlFlags |= FigmaQml::AltFontMatch;
            if(parser.isSet(figmaFontParameter))
                qmlFlags |= FigmaQml::KeepFigmaFontName;
            if(parser.isSet(qulmodeParameter))
                qmlFlags |= FigmaQml::QulMode;
            if(parser.isSet(staticCodeParameter))
                qmlFlags |= FigmaQml::StaticCode;
            if(parser.isSet(importsParameter)) {
                QMap<QString, QVariant> imports;
                const auto p = parser.value(importsParameter).split(';');
                for(const auto& i : p) {
                    const auto importLine = i.split(' ');
                    imports.insert(importLine[0], importLine[1]);
                }
                figmaQml->setProperty("imports", imports);
             }
        }

        if(!fontFolder.isEmpty() && !QDir(fontFolder).exists()) {
            ::print() << (QString("Error: Font folder not found \"%1\"").arg(fontFolder));
            return -1;
        }

        if(parser.isSet(fontMapParameter)) {
            const auto fonts = parseFontMap(parser.value(fontMapParameter), parser.isSet(altFontMatchParameter));
            for(const auto& [k, v] : fonts)
                figmaQml->setFontMapping(k, v);
        }

        if(parser.isSet(timedParameter))
            qmlFlags |= FigmaQml::Timed;

         if(!userToken.isEmpty())
             figmaGet->setProperty("userToken", userToken);
         if(!projectToken.isEmpty())
             figmaGet->setProperty("projectToken", projectToken);

         figmaQml->setProperty("flags", qmlFlags);
         figmaQml->setProperty("fontFolder", fontFolder);

         if(parser.isSet(imageDimensionMaxParameter))
            figmaQml->setProperty("imageDimensionMax", parser.value(imageDimensionMaxParameter));

         if(parser.isSet(throttleParameter))
            figmaGet->setProperty("throttle", parser.value(throttleParameter));
     }


     if(state & CmdLine) {
         auto start = QTime::currentTime();
         QObject::connect(figmaGet->downloadProgress(), &Downloads::bytesReceivedChanged, [&start]() {
             const auto now = QTime::currentTime();
             if(start.secsTo(now) > 1) {
                ::print() << "." << Qt::flush;
                start = now;
             }
         });
         QObject::connect(figmaQml.get(), &FigmaQml::warning, [](const QString& infoString ){
            ::print() << "\nInfo: " << infoString << Qt::endl;
         });
         QObject::connect(figmaQml.get(), &FigmaQml::warning, [](const QString& warningString) {
            ::print() << "\nWarning: " << warningString << Qt::endl;
         });
         QObject::connect(figmaQml.get(), &FigmaQml::error, [&app, &figmaGet, &supressErrors](const QString& errorString) {
            if(supressErrors)
                return;
            supressErrors = true;
            figmaGet->cancel();
            ::print() << "\nParser Error: " << errorString << Qt::endl;
            QTimer::singleShot(800, &app, [&app]() {
                app.exit(-2);
            }); //delay must be big enough so all threads notice app.exit side effect is to cease all eventloop.execs!
         });
         QObject::connect(figmaGet.get(), &FigmaGet::error, [&app, &figmaGet, &supressErrors](const QString& errorString) {
            if(supressErrors)
                return;
            supressErrors = true;
            figmaGet->cancel();
            ::print() << "\nConnection Error: " << errorString << Qt::endl;
            QTimer::singleShot(800, &app, [&app]() {
                app.exit(-3);
            });  //delay must be big enough so all threads notice app.exit side effect is to cease all eventloop.execs!
         });


         QObject::connect(figmaQml.get(), &FigmaQml::sourceCodeChanged, [&figmaQml, &figmaGet, output, &app, state/*, &onDataChange*/]() {
             int excode = 0;
             QEventLoop loop;
             QTimer exit;
             QObject::connect(&exit, &QTimer::timeout,[&]() { // correct way is to wait all...
                 if(!figmaGet->isReady())
                     return;
                 if(state & Store) {
                     const auto saveName = output.endsWith(".figmaqml") ? output : output + ".figmaqml";
                     if(figmaGet->store(saveName, figmaQml->property(FLAGS).toUInt(), figmaQml->property(IMPORTS).value<QVariantMap>())) {
                         ::print() << "\nStored to " << saveName << Qt::endl;
                     } else {
                         ::print() << "\nStore to " << saveName << " failed" << Qt::endl;
                          excode = -1;
                         }
                 } else if(!output.isEmpty()) {
                    if(figmaQml->saveAllQML(output)) {
                        ::print() << "\nSaved to " << output << Qt::endl;
                    } else {
                        ::print() << "\nSave to " << output << " failed" << Qt::endl;
                        excode = -1;
                    }
                 }
                 if(state & ShowFonts) {
                     const auto fonts = figmaQml->fonts();
                     const auto keys = fonts.keys();
                     for(const auto& k : keys) {
                         ::print() << "Font: " << k << "->" << fonts[k].toString() << Qt::endl;
                     }
                 }
                 loop.quit();
             });
             exit.start(400);
             loop.exec();
             QTimer::singleShot(0, &app, [&app, excode](){app.exit(excode);});
         });

         if(!restore.isEmpty()) {
             QObject::connect(figmaGet.get(), &FigmaGet::restored,
                              figmaQml.get(), [&figmaGet, &figmaQml](unsigned flags, const QVariantMap& imports) {

                 figmaQml->restore(flags, imports);
                 figmaQml->createDocumentSources(figmaGet->data());
             });
         } else {
             onDataChange = [&figmaGet, &figmaQml]() {
                 figmaQml->createDocumentSources(figmaGet->data());
             };
             figmaGet->update();
        }
     }

     if(!(state & CmdLine) && snapFile.isEmpty()) {

         QQuickStyle::setStyle(UI_STYLE);

         QObject::connect(figmaQml.get(), &FigmaQml::flagsChanged,
                          figmaQml.get(), [&figmaGet, &figmaQml]() {
             QSettings settings(COMPANY_NAME, PRODUCT_NAME);
             settings.setValue(FLAGS, figmaQml->property("flags").toUInt());
             figmaQml->createDocumentView(figmaGet->data(), true);
         });

         QObject::connect(figmaQml.get(), &FigmaQml::imageDimensionMaxChanged,
                          figmaQml.get(), [&figmaGet, &figmaQml]() {
             QSettings settings(COMPANY_NAME, PRODUCT_NAME);
             settings.setValue(IMAGEMAXSIZE, figmaQml->property("imageDimensionMax").toInt());
             figmaQml->createDocumentView(figmaGet->data(), true);
         });


         QObject::connect(figmaQml.get(), &FigmaQml::fontsChanged,
                          figmaQml.get(), [&figmaQml]() {
             QSettings settings(COMPANY_NAME, PRODUCT_NAME);
             settings.setValue(FONTMAP, figmaQml->property("fonts").toMap());
         });

         QObject::connect(figmaGet.get(), &FigmaGet::projectTokenChanged, [&figmaGet](){
             QSettings settings(COMPANY_NAME, PRODUCT_NAME);
             settings.setValue(PROJECT_TOKEN, figmaGet->property("projectToken").toString());
         });

         QObject::connect(figmaGet.get(), &FigmaGet::userTokenChanged, [&figmaGet](){
             QSettings settings(COMPANY_NAME, PRODUCT_NAME);
             settings.setValue(USER_TOKEN, figmaGet->property("userToken").toString());
         });

         QObject::connect(figmaQml.get(), &FigmaQml::importsChanged, [&figmaQml, &figmaGet]() {
             QSettings settings(COMPANY_NAME, PRODUCT_NAME);
             settings.setValue(IMPORTS, figmaQml->property("imports").toMap());
             figmaQml->createDocumentView(figmaGet->data(), true);
         });

         QObject::connect(figmaQml.get(), &FigmaQml::fontFolderChanged, [&figmaQml, &figmaGet]() {
             QSettings settings(COMPANY_NAME, PRODUCT_NAME);
             settings.setValue(FONTFOLDER, figmaQml->property("fontFolder").toString());
             figmaQml->createDocumentView(figmaGet->data(), true);
         });

         QObject::connect(figmaGet.get(), &FigmaGet::restored,
                          figmaQml.get(), [&figmaGet, &figmaQml](unsigned flags, const QVariantMap& imports) {
             figmaQml->restore(flags, imports);
             figmaQml->createDocumentView(figmaGet->data(), false);
         });

         QObject::connect(figmaQml.get(), &FigmaQml::refresh, figmaGet.get(), [&figmaQml, &figmaGet](){
              figmaQml->createDocumentView(figmaGet->data(), true);
         });

         QSettings settings(COMPANY_NAME, PRODUCT_NAME);

         figmaGet->setProperty("projectToken", settings.value(PROJECT_TOKEN));
         figmaGet->setProperty("userToken", settings.value(USER_TOKEN));
         figmaGet->setProperty("throttle", settings.value(THROTTLE, 300));
         figmaQml->setProperty("flags", settings.value(FLAGS, 0).toUInt());
         figmaQml->setProperty("imports", settings.value(IMPORTS, figmaQml->defaultImports()).toMap());

         figmaGet->setProperty("embedImages", settings.value(EMBED_IMAGES).toString());
         figmaQml->setProperty("imageDimensionMax", settings.value(IMAGEMAXSIZE, 1024).toInt());

         figmaQml->setProperty("fonts", settings.value(FONTMAP, QVariantMap()).toMap());

     }



    if(!snapFile.isEmpty()) {
         QObject::connect(figmaQml.get(), &FigmaQml::snapped, [&app]() {
              QTimer::singleShot(0, [&app](){app.quit();});
         });

         QObject::connect(figmaQml.get(), &FigmaQml::componentLoaded, [&figmaQml, &canvas, &element, &snapFile](int currentCanvas, int currentElement) {
             if(currentCanvas == canvas - 1 && currentElement == element - 1) {
                 emit figmaQml->takeSnap(snapFile, canvas - 1, element - 1);
             }
         });

         if(!restore.isEmpty()) {
             QObject::connect(figmaGet.get(), &FigmaGet::restored,
                              figmaQml.get(), [&figmaGet, &figmaQml](unsigned flags, const QVariantMap& imports) {
                  figmaQml->restore(flags, imports);
                  figmaQml->createDocumentView(figmaGet->data(), false);
             });
         } else {
             onDataChange = [&figmaGet, &figmaQml]() {
                 figmaQml->createDocumentView(figmaGet->data(), false);
             };
             figmaGet->update();
         }
    }


    QQmlApplicationEngine engine;
    FigmaQmlInterface::registerFigmaQmlSingleton(engine);

    if(!(state & CmdLine)) {
         onDataChange = [&figmaGet, &figmaQml]() {
                      figmaQml->createDocumentView(figmaGet->data(), true);
                  };

        QObject::connect(figmaQml.get(), &FigmaQml::documentCreated, figmaGet.get(), &FigmaGet::documentCreated);

        if(parser.isSet(showParameter)) {
                auto connection = std::make_shared<QMetaObject::Connection>();
                *connection = QObject::connect(figmaQml.get(), &FigmaQml::documentCreated, [&figmaQml, &canvas, &element, &app, connection] () {
                    if(figmaQml->isValid()) {
                         if(!figmaQml->setCurrentCanvas(canvas - 1)) {
                             ::print() << "Error: Invalid page " << canvas << " of " << figmaQml->canvasCount() << Qt::endl;
                             QTimer::singleShot(100, [&app](){app.exit(-2);}); //ensure a correct thread
                             return;
                         }

                         if(!figmaQml->setCurrentElement(element - 1))  {
                             ::print() << "Error: Invalid view " << element << " of " << figmaQml->elementCount() << Qt::endl;
                             QTimer::singleShot(100, [&app](){app.exit(-2);});
                             return;
                         }
                    }
                    QObject::disconnect(*connection);
                });
        }

        //qmlRegisterSingletonType(QUrl("qrc:///FigmaQmlSingleton.qml"), "FigmaQmlInterface", 1, 0, "FigmaQmlSingleton");

        // //engine.rootContext()->setContextProperty("FigmaQmlSingleton", &figmaAccess);
         engine.rootContext()->setContextProperty("clipboard", &clipboard);
         engine.rootContext()->setContextProperty("figmaGet", figmaGet.get());
         engine.rootContext()->setContextProperty("figmaQml", figmaQml.get());
         engine.rootContext()->setContextProperty("figmaDownload", figmaGet->downloadProgress());
         engine.rootContext()->setContextProperty("figmaQmlVersionNumber", versionNumber);

         engine.rootContext()->setContextProperty("qtVersion6",
#ifdef QT5
          false);
#else
          true);
#endif
         engine.rootContext()->setContextProperty("isWebAssembly",
#ifdef Q_CC_EMSCRIPTEN
          true);
#else
          false);
#endif

         engine.rootContext()->setContextProperty("has_qul",
#ifdef HAS_QUL
                                                  true);
#else
                                                  false);
#endif

         engine.rootContext()->setContextProperty("has_execute",
#ifdef HAS_EXECUTE
                                                  true);
#else
                                                  false);
#endif

         QObject::connect(figmaQml.get(), &FigmaQml::elementChanged, [&engine](){
             engine.clearComponentCache();
         });

         const QUrl qml_source("qrc:/main.qml");

         QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &engine, [qml_source, &figmaGet, &engine](QObject *object, const QUrl &url) {
             if(object && url == qml_source) {
                 // UI loaded - I should implement READ/WRITE for these properties, but this is the only place used in C++
                 // if(figmaGet->property("projectToken").toString().isEmpty() && figmaGet->property("userToken").toString().isEmpty()) {
                 //     QMetaObject::invokeMethod(engine.rootObjects()[0], "openTokens");
                 // }
                 QMetaObject::invokeMethod(engine.rootObjects()[0], "openTokens");

             }
         });

        auto* singleton = FigmaQmlSingleton::instance(engine);
        QObject::connect(figmaQml.get(), &FigmaQml::externalLoadersApplied, singleton, &FigmaQmlSingleton::setSource);

        engine.load(qml_source);

     }

     QObject::connect(figmaGet.get(), &FigmaGet::dataChanged,
                      &onDataChange, &Execute::execute, Qt::QueuedConnection);


     if(!restore.isEmpty()) {
         figmaGet->restore(restore);
     }

    //qDebug()  << "FOO: start app" << dir.path();
    const auto return_value = app.exec();
    //qDebug()  << "FOO: exit app" << dir.path();
    return return_value;
}


