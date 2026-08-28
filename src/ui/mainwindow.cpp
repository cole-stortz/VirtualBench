#include "src/ui/mainwindow.h"
#include "src/ui/canvaswidget.h"
#include "src/core/circuit/componentitem.h"
#include "src/appsettings.h"
#include "src/ui/editor/sketchlinter.h"
#include <qnamespace.h>
#include <qpushbutton.h>
#include <regex>
#include <algorithm>
#include <cmath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QFileDialog>
#include <QStatusBar>
#include <QFont>
#include <QLabel>
#include <QFrame>
#include <QCoreApplication>
#include <QApplication>
#include <QTextEdit>
#include <QInputDialog>
#include <QDir>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QToolTip>
#include <QIcon>
#include <QWheelEvent>
#include <QMessageBox>
#include <QRegularExpression>
#include <QMenuBar>
#include <QTextBrowser>
#include <QDialog>
#include <QListWidget>
#include <QTreeWidget>
#include <QDialogButtonBox>
#include "src/ui/sketchmanifest.h"

// Font size intentionally left out of the QSS -- set via setFont() below so zoomIn()/zoomOut() (which adjust
// the QFont directly) aren't fought by a cascaded font-size. Was "13px" in the old stylesheet, converted to points.
static const int DEFAULT_EDITOR_FONT_SIZE = 10;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    updateWindowTitle();
    resize(1280, 720);
    setMinimumSize(900, 600);

    QWidget*     central = new QWidget(this);
    QVBoxLayout* layout  = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setCentralWidget(central);

    QSettings settings = appSettings();
    compilerPath_ = settings.value("compiler/path", "").toString();
    projectRoot_  = settings.value("compiler/project_root", "").toString();
    defaultSketchLocation_ = settings.value("sketches/default_location",
        QCoreApplication::applicationDirPath() + "/sketches").toString();

    QString boardName = settings.value("board/name", "Arduino Uno").toString();
    activeProfile_ = boardProfileForName(boardName);

    if (compilerPath_.isEmpty() || projectRoot_.isEmpty()) {
        SettingsDialog dialog(this);
        QString detectedPath, detectedRoot;
        if (SettingsDialog::autoDetectCompiler(detectedPath, detectedRoot)) {
            dialog.setCompilerPath(detectedPath);
            dialog.setProjectRoot(detectedRoot);
        }
        if (dialog.exec() == QDialog::Accepted) {
            compilerPath_ = dialog.compilerPath();
            projectRoot_  = dialog.projectRoot();
            settings.setValue("compiler/path", compilerPath_);
            settings.setValue("compiler/project_root", projectRoot_);
            keybinds_.apply(dialog.keybinds());
        }
    }

    setupToolbar(central, layout);
    boardLabel_->setText(activeProfile_.name);

    setupMainArea(central, layout);  // variableWatch_ must exist before the connect below
    editorPanel_->setVisible(settings.value("window/panel_editor", true).toBool());
    canvasPanel_->setVisible(settings.value("window/panel_canvas", true).toBool());
    debugPanel_->setVisible(settings.value("window/panel_debug", true).toBool());
    canvasWidget_->setProfile(activeProfile_);
    darkTheme_ = settings.value("canvas/dark_theme", true).toBool();
    setAppTheme(darkTheme_);

    sketchThread_ = new SketchThread(this);
    sketchThread_->setProfile(activeProfile_);
    analogNoise_ = settings.value("simulation/analog_noise", false).toBool();
    sketchThread_->setAnalogNoise(analogNoise_);
    autoCompileOnSave_ = settings.value("editor/auto_compile_on_save", false).toBool();
    connect(sketchThread_, &SketchThread::serialOutput,
            this, &MainWindow::onSerialOutput);
    connect(sketchThread_, &SketchThread::serial1Output,
            this, &MainWindow::onSerial1Output);
    connect(sketchThread_, &SketchThread::serial2Output,
            this, &MainWindow::onSerial2Output);
    connect(sketchThread_, &SketchThread::softSerialOutput,
            this, &MainWindow::onSoftSerialOutput);
    connect(sketchThread_, &SketchThread::pinChanged,
            this, &MainWindow::onPinChanged);
    connect(sketchThread_, &SketchThread::assertResult,
            this, [this](bool pass, double time, QString message) {
                serialMonitor_->appendPlainText(QString("[TIMELINE] %1 [t=%2] %3")
                    .arg(pass ? "PASS" : "FAIL").arg(time, 0, 'f', 2).arg(message));
            });
    connect(sketchThread_, &SketchThread::timelineFinished,
            this, [this](bool anyFailed, int passCount, int totalCount) {
                Q_UNUSED(anyFailed);
                statusBar()->showMessage(
                    QString("Timeline finished: %1/%2 asserts passed").arg(passCount).arg(totalCount));
            });
    connect(sketchThread_, &SketchThread::sketchReloaded,
            this, &MainWindow::onSketchReloaded);
    connect(sketchThread_, &SketchThread::loadFailed,
            this, &MainWindow::onLoadFailed);
    connect(sketchThread_, &SketchThread::sketchCrashed,
            this, [this](const QString& reason) {
                serialMonitor_->appendPlainText("ERROR: " + reason + "\n");
                statusBar()->showMessage("Sketch crashed");
                setRunTriggersEnabled(true);
                stopButton_->setEnabled(false);
                canvasWidget_->setSketchRunning(false);
                finishRecordingIfActive();
            });
    connect(sketchThread_, &SketchThread::watchdogReset,
            this, [this]() {
                serialMonitor_->appendPlainText("WARNING: Watchdog reset — wdt_reset() was not called in time\n");
                onStopClicked();
                onRunClicked();
            });
    connect(sketchThread_, &SketchThread::sleepChanged,
            this, [this](bool sleeping) {
                statusBar()->showMessage(sleeping ? "Sleeping…" : "Running");
            });
    connect(sketchThread_, &SketchThread::variableChanged,
            variableWatch_, &VariableWatch::onVariableChanged);
    connect(sketchThread_, &SketchThread::lcdPrint,
            canvasWidget_, &CanvasWidget::updateLcdText);
    connect(sketchThread_, &SketchThread::matrixRowChanged,
            canvasWidget_, &CanvasWidget::updateMatrixRow);
    connect(sketchThread_, &SketchThread::neopixelShow,
            canvasWidget_, &CanvasWidget::updateNeopixelShow);
    connect(sketchThread_, &SketchThread::oledDisplay,
            canvasWidget_, &CanvasWidget::updateOledDisplay);
    connect(devicesPanel_, &DevicesPanel::deviceChanged,
            this, [this](int address, std::vector<uint8_t> bytes) {
                sketchThread_->injectWireDevice(address, bytes);
            });
    connect(spiPanel_, &SpiPanel::bytesChanged,
            this, [this](std::vector<uint8_t> bytes) {
                sketchThread_->injectSpiBytes(bytes);
            });
    connect(variableWatch_, &VariableWatch::watchListChanged,
            this, [this](std::vector<std::pair<QString, QString>> vars) {
                sketchThread_->setWatchList(vars);
            });

    QShortcut* save_shortcut = new QShortcut(keybinds_.load(settings, "save", QKeySequence::Save), this);
    connect(save_shortcut, &QShortcut::activated, this, &MainWindow::onSaveClicked);
    keybinds_.registerShortcut("save", save_shortcut);
    QShortcut* save_as_shortcut = new QShortcut(keybinds_.load(settings, "save_as", QKeySequence::SaveAs), this);
    connect(save_as_shortcut, &QShortcut::activated, this, &MainWindow::onSaveAsClicked);
    keybinds_.registerShortcut("save_as", save_as_shortcut);

    // Mirrors runButton_'s own disabled-while-running guard -- a shortcut has
    // no disabled state of its own to stop it firing mid-run.
    QShortcut* run_shortcut = new QShortcut(keybinds_.load(settings, "run", QKeySequence(Qt::CTRL | Qt::Key_R)), this);
    connect(run_shortcut, &QShortcut::activated, this, [this]() {
        if (runButton_->isEnabled()) onRunClicked();
    });
    keybinds_.registerShortcut("run", run_shortcut);

    // Explicit key sequences instead of QKeySequence::ZoomIn/ZoomOut -- its platform-standard key collides with
    // literal "Ctrl+=" here, and two QShortcuts sharing a sequence go ambiguous (neither fires). "+" is a fixed alias.
    QShortcut* zoom_in_shortcut = new QShortcut(keybinds_.load(settings, "editor_zoom_in", QKeySequence(Qt::CTRL | Qt::Key_Equal)), this);
    connect(zoom_in_shortcut, &QShortcut::activated, this, [this]() { adjustEditorZoom(1); });
    keybinds_.registerShortcut("editor_zoom_in", zoom_in_shortcut);
    QShortcut* zoom_in_shortcut_shift = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Equal), this);
    connect(zoom_in_shortcut_shift, &QShortcut::activated, this, [this]() { adjustEditorZoom(1); });
    QShortcut* zoom_out_shortcut = new QShortcut(keybinds_.load(settings, "editor_zoom_out", QKeySequence(Qt::CTRL | Qt::Key_Minus)), this);
    connect(zoom_out_shortcut, &QShortcut::activated, this, [this]() { adjustEditorZoom(-1); });
    keybinds_.registerShortcut("editor_zoom_out", zoom_out_shortcut);
    QShortcut* zoom_reset_shortcut = new QShortcut(keybinds_.load(settings, "editor_zoom_reset", QKeySequence(Qt::CTRL | Qt::Key_0)), this);
    connect(zoom_reset_shortcut, &QShortcut::activated, this, &MainWindow::resetEditorZoom);
    keybinds_.registerShortcut("editor_zoom_reset", zoom_reset_shortcut);

    // Same unshifted/shifted pairing as the editor zoom shortcuts above, on Alt
    // instead of Ctrl so it doesn't collide with editor font zoom.
    QShortcut* canvas_zoom_in_shortcut = new QShortcut(keybinds_.load(settings, "canvas_zoom_in", QKeySequence(Qt::ALT | Qt::Key_Equal)), this);
    connect(canvas_zoom_in_shortcut, &QShortcut::activated, this, [this]() { canvasWidget_->zoomIn(); });
    keybinds_.registerShortcut("canvas_zoom_in", canvas_zoom_in_shortcut);
    QShortcut* canvas_zoom_in_shortcut_shift = new QShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_Equal), this);
    connect(canvas_zoom_in_shortcut_shift, &QShortcut::activated, this, [this]() { canvasWidget_->zoomIn(); });
    QShortcut* canvas_zoom_out_shortcut = new QShortcut(keybinds_.load(settings, "canvas_zoom_out", QKeySequence(Qt::ALT | Qt::Key_Minus)), this);
    connect(canvas_zoom_out_shortcut, &QShortcut::activated, this, [this]() { canvasWidget_->zoomOut(); });
    keybinds_.registerShortcut("canvas_zoom_out", canvas_zoom_out_shortcut);

    connect(codeEditor_->document(), &QTextDocument::modificationChanged,
            this, [this](bool) { updateWindowTitle(); });

    QShortcut* find_shortcut = new QShortcut(keybinds_.load(settings, "find", QKeySequence::Find), this);
    connect(find_shortcut, &QShortcut::activated, findReplaceBar_, &FindReplaceBar::showBar);
    keybinds_.registerShortcut("find", find_shortcut);

    // code_completion, duplicate_line, and comment_toggle have no QShortcut -- they're raw key comparisons in
    // EditorWithLines::keyPressEvent, so just seed keybindSeq_ and push the sequence down to the editor.
    keybinds_.load(settings, "code_completion", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Space));
    keybinds_.load(settings, "duplicate_line", QKeySequence(Qt::CTRL | Qt::Key_D));
    keybinds_.load(settings, "comment_toggle", QKeySequence(Qt::CTRL | Qt::Key_Slash));
    codeEditor_->setActionKeybinds(keybinds_.value("code_completion"),
                                    keybinds_.value("duplicate_line"),
                                    keybinds_.value("comment_toggle"));
    connect(codeEditor_, &EditorWithLines::completionRequested,
            this, &MainWindow::showCompletionPopup);
    connect(codeEditor_, &EditorWithLines::dotTyped,
            this, &MainWindow::showMemberCompletionPopup);

    // Autosave every 30s, but only for a sketch with a real (non-scratch) path
    // and only if there are actually unsaved changes to write
    autosaveTimer_ = new QTimer(this);
    autosaveTimer_->setInterval(30000);
    connect(autosaveTimer_, &QTimer::timeout, this, [this]() {
        bool has_real_path = !currentSketchPath_.isEmpty()
            && !currentSketchPath_.startsWith(QDir::tempPath());
        if (!has_real_path || !codeEditor_->document()->isModified()) return;

        QFile file(currentSketchPath_ + ".autosave");
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(codeEditor_->toPlainText().toUtf8());
            file.close();
        }
    });
    autosaveTimer_->start();

    setupMenuBar();

    statusBar()->showMessage("Ready");
}

MainWindow::~MainWindow() {
    if (sketchThread_)
        sketchThread_->stopSketch();
}

// Nothing unsaved on exit means nothing to recover -- drop any pending autosave. Otherwise leave it for
// checkForAutosaveRecovery to offer back next time this sketch is opened.
void MainWindow::closeEvent(QCloseEvent* event) {
    bool has_real_path = !currentSketchPath_.isEmpty()
        && !currentSketchPath_.startsWith(QDir::tempPath());
    if (has_real_path && !codeEditor_->document()->isModified())
        QFile::remove(currentSketchPath_ + ".autosave");
    QMainWindow::closeEvent(event);
}

void MainWindow::setupMenuBar() {
    QMenuBar* menuBar = new QMenuBar(this);

    // File
    QMenu* fileMenu = menuBar->addMenu("&File");
    fileMenu->addAction("New Sketch", this, &MainWindow::onNewSketch);
    fileMenu->addAction("Open Sketch...", this, &MainWindow::onOpenClicked);
    fileMenu->addAction("Open Example...", this, &MainWindow::onOpenExample);
    QMenu* recentMenu = fileMenu->addMenu("Recent Sketches");
    connect(recentMenu, &QMenu::aboutToShow, this, [this, recentMenu]() {
        populateRecentMenu(recentMenu);
    });
    fileMenu->addSeparator();
    fileMenu->addAction("Save", this, &MainWindow::onSaveClicked);
    fileMenu->addAction("Save As...", this, &MainWindow::onSaveAsClicked);
    fileMenu->addSeparator();
    fileMenu->addAction("Settings...", this, &MainWindow::onSettingsClicked);
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);

    // Edit -- codeEditor_ is a QPlainTextEdit subclass, so undo/redo/cut/copy/
    // paste/selectAll are free via QPlainTextEdit's own slots, no new handlers needed
    QMenu* editMenu = menuBar->addMenu("&Edit");
    editMenu->addAction("Undo", QKeySequence::Undo, codeEditor_, &QPlainTextEdit::undo);
    editMenu->addAction("Redo", QKeySequence::Redo, codeEditor_, &QPlainTextEdit::redo);
    editMenu->addSeparator();
    editMenu->addAction("Cut", QKeySequence::Cut, codeEditor_, &QPlainTextEdit::cut);
    editMenu->addAction("Copy", QKeySequence::Copy, codeEditor_, &QPlainTextEdit::copy);
    editMenu->addAction("Paste", QKeySequence::Paste, codeEditor_, &QPlainTextEdit::paste);
    editMenu->addSeparator();
    editMenu->addAction("Select All", QKeySequence::SelectAll, codeEditor_, &QPlainTextEdit::selectAll);
    editMenu->addSeparator();
    editMenu->addAction("Find", findReplaceBar_, &FindReplaceBar::showBar);
    editMenu->addSeparator();
    editMenu->addAction(QString("Duplicate Line\t%1").arg(keybinds_.value("duplicate_line").toString()),
        codeEditor_, &EditorWithLines::duplicateCurrentLine);
    editMenu->addAction(QString("Toggle Comment\t%1").arg(keybinds_.value("comment_toggle").toString()),
        codeEditor_, &EditorWithLines::toggleCommentSelection);

    // View
    QMenu* viewMenu = menuBar->addMenu("&View");
    QAction* darkThemeAction = viewMenu->addAction("Dark Theme");
    darkThemeAction->setCheckable(true);
    darkThemeAction->setChecked(darkTheme_);
    connect(darkThemeAction, &QAction::toggled, this, &MainWindow::setAppTheme);
    viewMenu->addSeparator();
    viewMenu->addAction(QString("Zoom In (Editor)\t%1").arg(keybinds_.value("editor_zoom_in").toString()),
        this, [this]() { adjustEditorZoom(1); });
    viewMenu->addAction(QString("Zoom Out (Editor)\t%1").arg(keybinds_.value("editor_zoom_out").toString()),
        this, [this]() { adjustEditorZoom(-1); });
    viewMenu->addAction(QString("Reset Zoom (Editor)\t%1").arg(keybinds_.value("editor_zoom_reset").toString()),
        this, &MainWindow::resetEditorZoom);
    viewMenu->addSeparator();
    viewMenu->addAction(QString("Zoom In (Canvas)\t%1").arg(keybinds_.value("canvas_zoom_in").toString()),
        this, [this]() { canvasWidget_->zoomIn(); });
    viewMenu->addAction(QString("Zoom Out (Canvas)\t%1").arg(keybinds_.value("canvas_zoom_out").toString()),
        this, [this]() { canvasWidget_->zoomOut(); });
    viewMenu->addSeparator();
    QAction* layoutModeAction = viewMenu->addAction("Layout Mode");
    layoutModeAction->setCheckable(true);
    connect(layoutModeAction, &QAction::toggled, layoutButton_, &QPushButton::setChecked);
    connect(layoutButton_, &QPushButton::toggled, layoutModeAction, &QAction::setChecked);

    // Window -- toggle panel/tab visibility, persisted to settings.ini
    QMenu* windowMenu = menuBar->addMenu("&Window");
    QSettings windowSettings = appSettings();

    auto addPanelToggle = [&](const QString& label, QWidget* panel, const QString& key) {
        QAction* action = windowMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(windowSettings.value(key, true).toBool());
        connect(action, &QAction::toggled, this, [this, panel, key](bool visible) {
            panel->setVisible(visible);
            QSettings settings = appSettings();
            settings.setValue(key, visible);
        });
        return action;
    };
    editorPanelAction_ = addPanelToggle("Editor", editorPanel_, "window/panel_editor");
    canvasPanelAction_ = addPanelToggle("Canvas", canvasPanel_, "window/panel_canvas");
    debugPanelAction_  = addPanelToggle("Debug Panel", debugPanel_, "window/panel_debug");

    windowMenu->addSeparator();
    for (int i = 0; i < debugTabToggles_.size(); ++i) {
        DebugTabEntry& entry = debugTabToggles_[i];
        QAction* action = windowMenu->addAction(entry.label);
        action->setCheckable(true);
        action->setChecked(debugTabs_->indexOf(entry.widget) != -1);
        connect(action, &QAction::toggled, this, [this, i](bool visible) {
            setDebugTabVisible(debugTabToggles_[i], visible);
        });
        entry.action = action;
    }

    windowMenu->addSeparator();
    windowMenu->addAction("Reset Layout", this, &MainWindow::resetWindowLayout);

    // Board -- quick switch without opening the full Settings dialog
    QMenu* boardMenu = menuBar->addMenu("&Board");
    QActionGroup* boardGroup = new QActionGroup(this);
    boardGroup->setExclusive(true);
    auto addBoardAction = [&](const BoardProfile& profile) {
        QAction* action = boardMenu->addAction(profile.name);
        action->setCheckable(true);
        action->setChecked(QString(activeProfile_.name) == QString(profile.name));
        boardGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, profile]() {
            applyBoardChange(profile);
        });
    };
    addBoardAction(BOARD_UNO);
    addBoardAction(BOARD_NANO);
    addBoardAction(BOARD_MEGA);
    addBoardAction(BOARD_DUE);
    addBoardAction(BOARD_TEENSY);

    // Run
    QMenu* runMenu = menuBar->addMenu("&Run");
    runMenu->addAction("Run", this, [this]() { if (runButton_->isEnabled()) onRunClicked(); });
    runMenu->addAction("Stop", this, &MainWindow::onStopClicked);
    runMenu->addSeparator();
    runMenu->addAction("Reset Canvas Layout", this, [this]() { canvasWidget_->resetLayout(); });

    // Help
    QMenu* helpMenu = menuBar->addMenu("&Help");
    helpMenu->addAction("About VEMCODE", this, [this]() {
        QMessageBox::about(this, "About VEMCODE",
            "VEMCODE — Virtual Embedded Machine Code Development Environment\n\n"
            "An Arduino/Teensy firmware simulator.");
    });
    helpMenu->addSeparator();
    helpMenu->addAction("Architecture", this, [this]() { showDocViewer("Architecture", "ARCHITECTURE.md"); });
    helpMenu->addAction("Sketch Guide", this, [this]() { showDocViewer("Sketch Guide", "SKETCH_GUIDE.md"); });
    helpMenu->addAction("Adding Components", this, [this]() { showDocViewer("Adding Components", "ADDING_COMPONENTS.md"); });
    helpMenu->addAction("API Reference", this, [this]() { showDocViewer("API Reference", "API_REFERENCE.md"); });
    helpMenu->addAction("Roadmap", this, [this]() { showDocViewer("Roadmap", "ROADMAP.md"); });

    setMenuBar(menuBar);
}

// Toolbar -- Run, Stop, Open, Save, board label
void MainWindow::setupToolbar(QWidget* parent, QVBoxLayout* layout) {
    QWidget*     toolbar      = new QWidget(parent);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(8, 6, 8, 6);
    toolbarLayout->setSpacing(6);
    toolbar->setObjectName("toolbar");
    toolbar->setFixedHeight(42);

    QLabel* logo = new QLabel(toolbar);
    logo->setPixmap(QIcon(":/logo.svg").pixmap(36, 36));
    toolbarLayout->addWidget(logo);
    toolbarLayout->addSpacing(6);

    QLabel* title = new QLabel("VEMCODE", toolbar);
    title->setObjectName("appTitle");
    toolbarLayout->addWidget(title);
    toolbarLayout->addSpacing(8);

    runButton_ = new QPushButton("Run", toolbar);
    runButton_->setFixedSize(64, 26);
    runButton_->setObjectName("btnRun");
    connect(runButton_, &QPushButton::clicked, this, &MainWindow::onRunClicked);
    toolbarLayout->addWidget(runButton_);

    stopButton_ = new QPushButton("Stop", toolbar);
    stopButton_->setFixedSize(64, 26);
    stopButton_->setEnabled(false);
    stopButton_->setObjectName("btnStop");
    connect(stopButton_, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    toolbarLayout->addWidget(stopButton_);

    recordButton_ = new QPushButton("Record", toolbar);
    recordButton_->setFixedSize(64, 26);
    recordButton_->setObjectName("btnRecord");
    recordButton_->setToolTip("Run while capturing canvas interactions and Serial sends;\n"
                               "exports a .timeline file when you click Stop");
    connect(recordButton_, &QPushButton::clicked, this, &MainWindow::onRecordClicked);
    toolbarLayout->addWidget(recordButton_);

    playTimelineButton_ = new QPushButton("Play Timeline", toolbar);
    playTimelineButton_->setFixedSize(96, 26);
    playTimelineButton_->setObjectName("btnPlayTimeline");
    playTimelineButton_->setToolTip("Pick a .timeline file and run the sketch driven by it");
    connect(playTimelineButton_, &QPushButton::clicked, this, &MainWindow::onPlayTimelineClicked);
    toolbarLayout->addWidget(playTimelineButton_);

    QLabel* speedLabel = new QLabel("Speed:", toolbar);
    speedLabel->setProperty("role", "muted-label");
    toolbarLayout->addWidget(speedLabel);

    speedSlider_ = new QSlider(Qt::Horizontal, toolbar);
    speedSlider_->setRange(1, 25);
    speedSlider_->setValue(13); // dead center of 1-25 -- see speedForSliderValue()
    speedSlider_->setFixedWidth(80);
    speedSlider_->setToolTip("Simulation speed (1.0x = normal, double-click to reset)");
    speedSlider_->installEventFilter(this);
    connect(speedSlider_, &QSlider::valueChanged, this, &MainWindow::onSpeedChanged);
    toolbarLayout->addWidget(speedSlider_);

    toolbarLayout->addStretch();

    boardLabel_ = new QLabel("", toolbar);
    boardLabel_->setObjectName("boardLabel");
    toolbarLayout->addWidget(boardLabel_);

    layout->addWidget(toolbar);
}

// Main area -- horizontal splitter: editor | (canvas + debug)
void MainWindow::setupMainArea(QWidget* parent, QVBoxLayout* layout) {
    mainSplitter_ = new QSplitter(Qt::Horizontal, parent);
    mainSplitter_->setHandleWidth(1);
    editorPanel_ = buildEditorPanel();
    mainSplitter_->addWidget(editorPanel_);

    rightSplitter_ = new QSplitter(Qt::Vertical, mainSplitter_);
    rightSplitter_->setHandleWidth(1);
    canvasPanel_ = buildCanvasPanel();
    rightSplitter_->addWidget(canvasPanel_);
    debugPanel_ = buildDebugPanel();
    rightSplitter_->addWidget(debugPanel_);
    rightSplitter_->setSizes({300, 220});

    mainSplitter_->addWidget(rightSplitter_);
    mainSplitter_->setSizes({380, 900});

    layout->addWidget(mainSplitter_);
}

// Editor panel (left)
QWidget* MainWindow::buildEditorPanel() {
    QWidget*     panel  = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QLabel* header = new QLabel("  SKETCH EDITOR");
    header->setFixedHeight(24);
    header->setProperty("role", "panel-header");
    layout->addWidget(header);

    codeEditor_ = new EditorWithLines();
    highlighter_ = new CodeHighlighter(codeEditor_->document());
    codeEditor_->setObjectName("codeEditor");
    codeEditor_->setLineWrapMode(QPlainTextEdit::NoWrap);

    QFont editorFont("Courier New", DEFAULT_EDITOR_FONT_SIZE);
    editorFont.setStyleHint(QFont::Monospace);
    codeEditor_->setFont(editorFont);

    QFontMetrics metrics(codeEditor_->font());
    codeEditor_->setTabStopDistance(4 * metrics.horizontalAdvance(' '));
    codeEditor_->installEventFilter(this);
    connect(codeEditor_, &QPlainTextEdit::cursorPositionChanged,
            this, &MainWindow::updateBracketMatch);

    // Find & Replace bar -- hidden until Ctrl+F
    findReplaceBar_ = new FindReplaceBar(codeEditor_);
    findReplaceBar_->setHighlightColor(highlightColors_.find_match_bg);
    connect(findReplaceBar_, &FindReplaceBar::matchesChanged, this,
            [this](QList<QTextEdit::ExtraSelection> selections) {
                findSelections_ = selections;
                refreshExtraSelections();
            });
    connect(findReplaceBar_, &FindReplaceBar::statusMessage, this,
            [this](const QString& text) { statusBar()->showMessage(text); });
    layout->addWidget(findReplaceBar_);

    // Auto Complete
    completer_ = new QCompleter(this);
    completer_->setWidget(codeEditor_);
    completer_->setCompletionMode(QCompleter::PopupCompletion);
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    connect(completer_, QOverload<const QString&>::of(&QCompleter::activated),
            this, &MainWindow::insertCompletion);
    completer_->popup()->installEventFilter(this);

    // Auto-popup: after typing >= 3 characters of a word, wait 1.5s idle then show the same popup Ctrl+Shift+Space does.
    idleCompletionTimer_ = new QTimer(this);
    idleCompletionTimer_->setSingleShot(true);
    idleCompletionTimer_->setInterval(1500);
    connect(idleCompletionTimer_, &QTimer::timeout, this, [this]() {
        if (!codeEditor_->hasFocus() || completer_->popup()->isVisible()) return;
        QTextCursor tc = codeEditor_->textCursor();
        tc.select(QTextCursor::WordUnderCursor);
        if (tc.selectedText().length() >= 3)
            showCompletionPopup();
    });
    connect(codeEditor_, &QPlainTextEdit::textChanged, this, [this]() {
        if (!codeEditor_->hasFocus()) {
            idleCompletionTimer_->stop();
            return;
        }
        QTextCursor tc = codeEditor_->textCursor();
        tc.select(QTextCursor::WordUnderCursor);
        QString word = tc.selectedText();

        // Popup already open (flat list or member-narrowed) -- just refine its prefix, don't retrigger the idle timer.
        if (completer_->popup()->isVisible()) {
            idleCompletionTimer_->stop();
            completer_->setCompletionPrefix(word);
            if (completer_->completionCount() == 0)
                completer_->popup()->hide();
            else
                completer_->popup()->setCurrentIndex(completer_->completionModel()->index(0, 0));
            return;
        }

        if (word.length() >= 3)
            idleCompletionTimer_->start();
        else
            idleCompletionTimer_->stop();
    });

    lineNumbers_ = new LineNumberArea(codeEditor_);
    lineNumbers_->show();

    codeEditor_->setPlainText(
        "#define LED_PIN 13\n\n"
        "void setup() {\n"
        "    Serial.begin(9600);\n"
        "    pinMode(LED_PIN, OUTPUT);\n"
        "    Serial.println(\"VEMCODE ready\");\n"
        "}\n\n"
        "void loop() {\n"
        "    digitalWrite(LED_PIN, HIGH);\n"
        "    delay(1000);\n"
        "    digitalWrite(LED_PIN, LOW);\n"
        "    delay(1000);\n"
        "    Serial.println(\"Blink\");\n"
        "}\n"
    );

    layout->addWidget(codeEditor_);
    return panel;
}

// Canvas panel (top right)
QWidget* MainWindow::buildCanvasPanel() {
    QWidget*     panel  = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QWidget*     header       = new QWidget();
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    header->setFixedHeight(24);
    header->setProperty("role", "panel-header");
    headerLayout->setContentsMargins(8, 0, 6, 0);
    headerLayout->setSpacing(0);

    QLabel* headerLabel = new QLabel("CIRCUIT CANVAS");
    headerLabel->setStyleSheet("border: none; background: transparent;");
    headerLayout->addWidget(headerLabel);
    headerLayout->addStretch();

    QPushButton* addCompButton = new QPushButton("Add/Remove", header);
    addCompButton->setFixedSize(96, 18);
    addCompButton->setProperty("role", "toggle");
    addCompButton->setToolTip("Add/Remove Components injecting or removing code in the sketch");
    connect(addCompButton, &QPushButton::clicked, this, [this]() {
        AddComponentDialog dialog(this);
        dialog.exec();
    });
    headerLayout->addWidget(addCompButton);

    headerLayout->addSpacing(4);

    layoutButton_ = new QPushButton("Layout", header);
    layoutButton_->setCheckable(true);
    layoutButton_->setFixedSize(56, 18);
    layoutButton_->setProperty("role", "toggle");
    layoutButton_->setToolTip("Toggle layout mode to drag components to new positions");
    connect(layoutButton_, &QPushButton::toggled, this, &MainWindow::onLayoutToggled);
    headerLayout->addWidget(layoutButton_);

    headerLayout->addSpacing(4);

    QPushButton* resetLayoutButton = new QPushButton("Reset", header);
    resetLayoutButton->setFixedSize(48, 18);
    resetLayoutButton->setProperty("role", "toggle");
    resetLayoutButton->setToolTip("Reset components to their auto-arranged positions");
    connect(resetLayoutButton, &QPushButton::clicked, this, [this]() {
        canvasWidget_->resetLayout();
    });
    headerLayout->addWidget(resetLayoutButton);

    layout->addWidget(header);

    canvasWidget_ = new CanvasWidget();
    layout->addWidget(canvasWidget_);

    connect(canvasWidget_, &CanvasWidget::inputChanged,
            this, &MainWindow::onComponentInput);

    return panel;
}

// Serial panel -- extracted so it can be rebuilt on board profile change
QWidget* MainWindow::buildSerialPanel() {
    QWidget*     serialPanel  = new QWidget();
    QVBoxLayout* serialLayout = new QVBoxLayout(serialPanel);
    serialLayout->setContentsMargins(0, 0, 0, 0);
    serialLayout->setSpacing(0);

    serialMonitor_  = new QPlainTextEdit();
    serialMonitor1_ = nullptr;
    serialMonitor2_ = nullptr;
    serialMonitor_->setReadOnly(true);
    serialMonitor_->setMaximumBlockCount(2000);
    serialMonitor_->setProperty("role", "serial");

    int serialCount = activeProfile_.serial_count;

    if (serialCount >= 2) {
        QSplitter* serialSplitter = new QSplitter(Qt::Horizontal, serialPanel);
        serialSplitter->setHandleWidth(1);

        auto makePane = [&](QPlainTextEdit* mon, const QString& label, bool bordered) {
            QWidget*     pane   = new QWidget();
            QVBoxLayout* layout = new QVBoxLayout(pane);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);
            QLabel* lbl = new QLabel(label, pane);
            lbl->setFixedHeight(18);
            lbl->setProperty("role", bordered ? "port-label-bordered" : "port-label");
            layout->addWidget(lbl);
            layout->addWidget(mon);
            return pane;
        };

        serialSplitter->addWidget(makePane(serialMonitor_, "  Serial", false));

        serialMonitor1_ = new QPlainTextEdit();
        serialMonitor1_->setReadOnly(true);
        serialMonitor1_->setMaximumBlockCount(2000);
        serialMonitor1_->setProperty("role", "serial");
        serialSplitter->addWidget(makePane(serialMonitor1_, "  Serial1", true));

        if (serialCount >= 3) {
            serialMonitor2_ = new QPlainTextEdit();
            serialMonitor2_->setReadOnly(true);
            serialMonitor2_->setMaximumBlockCount(2000);
            serialMonitor2_->setProperty("role", "serial");
            serialSplitter->addWidget(makePane(serialMonitor2_, "  Serial2", true));
        }

        serialLayout->addWidget(serialSplitter);
    } else {
        serialLayout->addWidget(serialMonitor_);
    }

    QWidget*     inputRow    = new QWidget();
    QHBoxLayout* inputLayout = new QHBoxLayout(inputRow);
    inputLayout->setContentsMargins(4, 4, 4, 4);
    inputLayout->setSpacing(4);
    inputRow->setProperty("role", "input-row");
    inputRow->setFixedHeight(36);

    serialInput_ = new QLineEdit();
    serialInput_->setPlaceholderText("Send serial input...");
    inputLayout->addWidget(serialInput_);

    QPushButton* sendButton = new QPushButton("Send", inputRow);
    sendButton->setFixedWidth(50);
    sendButton->setProperty("role", "outline");
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::onSerialSend);
    connect(serialInput_, &QLineEdit::returnPressed, this, &MainWindow::onSerialSend);
    inputLayout->addWidget(sendButton);

    serialLayout->addWidget(inputRow);
    return serialPanel;
}

void MainWindow::rebuildSerialMonitors() {
    // Serial monitor is always debugTabToggles_[0]; its tab index can move
    // around if other tabs are hidden, and it may be hidden itself.
    DebugTabEntry& serialEntry = debugTabToggles_[0];
    int  savedTab   = debugTabs_->currentIndex();
    int  oldIndex   = debugTabs_->indexOf(serialEntry.widget);
    bool wasVisible = oldIndex != -1;
    QWidget* oldWidget = serialEntry.widget;

    if (wasVisible) debugTabs_->removeTab(oldIndex);
    oldWidget->deleteLater();
    serialEntry.widget = buildSerialPanel();
    serialEntry.widget->setParent(debugTabs_->parentWidget());
    if (wasVisible) {
        debugTabs_->insertTab(oldIndex, serialEntry.widget, serialEntry.label);
        debugTabs_->setCurrentIndex(savedTab);
    }
}

// Counts how many entries ahead of `entry` in debugTabToggles_ are currently visible, so it reinserts in relative order.
int MainWindow::debugTabInsertIndex(const DebugTabEntry& entry) const {
    int index = 0;
    for (const DebugTabEntry& e : debugTabToggles_) {
        if (&e == &entry) break;
        if (debugTabs_->indexOf(e.widget) != -1) index++;
    }
    return index;
}

void MainWindow::resetWindowLayout() {
    editorPanelAction_->setChecked(true);
    canvasPanelAction_->setChecked(true);
    debugPanelAction_->setChecked(true);
    for (DebugTabEntry& entry : debugTabToggles_)
        if (entry.action) entry.action->setChecked(true);

    mainSplitter_->setSizes({380, 900});
    rightSplitter_->setSizes({300, 220});
}

void MainWindow::setDebugTabVisible(DebugTabEntry& entry, bool visible) {
    int currentIndex = debugTabs_->indexOf(entry.widget);
    if (visible && currentIndex == -1) {
        debugTabs_->insertTab(debugTabInsertIndex(entry), entry.widget, entry.label);
    } else if (!visible && currentIndex != -1) {
        debugTabs_->removeTab(currentIndex);
    }
    QSettings settings = appSettings();
    settings.setValue(entry.settingsKey, visible);
}

// Debug panel (bottom right) -- tabbed: serial, timeline, watch
QWidget* MainWindow::buildDebugPanel() {
    QWidget*     panel  = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    debugTabs_ = new QTabWidget();
    debugTabs_->setMinimumHeight(100);

    signalTimeline_ = new SignalTimeline();
    serialPlotter_  = new SerialPlotter();
    variableWatch_  = new VariableWatch();
    devicesPanel_   = new DevicesPanel();
    spiPanel_       = new SpiPanel();

    debugTabToggles_ = {
        {"Serial monitor",  buildSerialPanel(), "window/tab_serial_monitor"},
        {"Signal timeline", signalTimeline_,    "window/tab_signal_timeline"},
        {"Serial plotter",  serialPlotter_,     "window/tab_serial_plotter"},
        {"Variable watch",  variableWatch_,     "window/tab_variable_watch"},
        {"I2C",             devicesPanel_,      "window/tab_i2c"},
        {"SPI",             spiPanel_,          "window/tab_spi"},
    };

    QSettings settings = appSettings();
    for (DebugTabEntry& entry : debugTabToggles_) {
        entry.widget->setParent(panel);  // owned even if not added as a tab below
        if (settings.value(entry.settingsKey, true).toBool())
            debugTabs_->addTab(entry.widget, entry.label);
    }

    layout->addWidget(debugTabs_);
    return panel;
}

// Slots
void MainWindow::onSerialOutput(QString text) {
    serialMonitor_->moveCursor(QTextCursor::End);
    serialMonitor_->insertPlainText(text);
    serialMonitor_->moveCursor(QTextCursor::End);
    if (serialPlotter_) serialPlotter_->ingest(text, simTimer_.elapsed());
}

void MainWindow::onSerial1Output(QString text) {
    if (!serialMonitor1_) return;
    serialMonitor1_->moveCursor(QTextCursor::End);
    serialMonitor1_->insertPlainText(text);
    serialMonitor1_->moveCursor(QTextCursor::End);
}

void MainWindow::onSerial2Output(QString text) {
    if (!serialMonitor2_) return;
    serialMonitor2_->moveCursor(QTextCursor::End);
    serialMonitor2_->insertPlainText(text);
    serialMonitor2_->moveCursor(QTextCursor::End);
}

void MainWindow::onSoftSerialOutput(int rxPin, QString text) {
    bool atStart = !softSerialLineStart_.contains(rxPin) || softSerialLineStart_[rxPin];
    QString out;
    for (QChar c : text) {
        if (atStart) {
            out += QString("[SW:%1] ").arg(rxPin);
            atStart = false;
        }
        out += c;
        if (c == '\n') atStart = true;
    }
    softSerialLineStart_[rxPin] = atStart;
    serialMonitor_->moveCursor(QTextCursor::End);
    serialMonitor_->insertPlainText(out);
    serialMonitor_->moveCursor(QTextCursor::End);
}

void MainWindow::onPinChanged(int pin, int value) {
    canvasWidget_->updatePin(pin, value);
    // Mark this pin as actively driven so the detector can distinguish
    // components actually wired up from ones only referenced in source
    detector_.confirm_pin(pin);
    signalTimeline_->addEvent(pin, value, simTimer_.elapsed());
    statusBar()->showMessage(
        QString("Pin %1 -> %2").arg(pin).arg(value ? "HIGH" : "LOW")
    );
}

void MainWindow::onComponentInput(int pin, int eventType, QVariant value) {
    if (timelineRecorder_.isActive())
        timelineRecorder_.recordComponentInput(pin, static_cast<ComponentEventType>(eventType),
            value, simTimer_.elapsed() / 1000.0, detector_.components());

    switch (static_cast<ComponentEventType>(eventType)) {
        case ComponentEventType::DigitalPress:
            sketchThread_->injectPin(pin, value.toInt());
            break;
        case ComponentEventType::BouncePress:
            sketchThread_->injectButtonBounce(pin, value.toInt());
            break;
        case ComponentEventType::AnalogValue:
            sketchThread_->injectAnalog(pin, value.toInt());
            break;
        case ComponentEventType::PulseUs:
            sketchThread_->injectPulseDuration(pin, value.toULongLong());
            break;
        case ComponentEventType::ColorRGB: {
            QVariantList c = value.toList();
            sketchThread_->injectColor(
                pin, c.value(3).toInt(), c.value(4).toInt(),
                c.value(0).toInt(), c.value(1).toInt(), c.value(2).toInt());
            break;
        }
        case ComponentEventType::KeypadWiring: {
            QVariantList w = value.toList();
            int num_cols = w.value(0).toInt();
            std::vector<int> colPins, rowPins;
            int i = 1;
            for (int n = 0; n < num_cols; ++n) colPins.push_back(w.value(i++).toInt());
            for (; i < w.size(); ++i) rowPins.push_back(w.value(i).toInt());
            sketchThread_->injectKeypadWiring(colPins, rowPins);
            break;
        }
        case ComponentEventType::KeypadPress: {
            QVariantList k = value.toList();
            sketchThread_->injectKeypadPress(k.value(0).toInt(), k.value(1).toInt(), k.value(2).toInt() != 0);
            break;
        }
        case ComponentEventType::DhtReading: {
            QVariantList d = value.toList();
            sketchThread_->injectDht(pin, (float)d.value(0).toDouble(), (float)d.value(1).toDouble());
            break;
        }
    }
}

void MainWindow::insertCompletion(const QString& completion) {
    QTextCursor tc = codeEditor_->textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    tc.insertText(completion);
    codeEditor_->setTextCursor(tc);
}

void MainWindow::onSketchReloaded() {
    serialMonitor_->appendPlainText("\n--- sketch reloaded ---\n");
    statusBar()->showMessage("Sketch hot-reloaded");
}

void MainWindow::onLoadFailed(QString reason) {
    serialMonitor_->appendPlainText("ERROR: " + reason + "\n");
    statusBar()->showMessage("Load failed");
    setRunTriggersEnabled(true);
    stopButton_->setEnabled(false);
    canvasWidget_->setSketchRunning(false);
    finishRecordingIfActive();
}

// Button handlers
void MainWindow::onRunClicked() {
    // If no file is open, save to a temp file and run that. Uses its own subfolder rather than tempPath()
    // directly -- the compiler scans the sketch's folder for extra .cpp files to link in (multi-file support).
    if (currentSketchPath_.isEmpty()) {
        QString temp_dir = QDir::tempPath() + "/vemcode_unsaved_sketch";
        QDir().mkpath(temp_dir);
        QString temp_path = temp_dir + "/vb_sketch.cpp";
        QFile temp_file(temp_path);
        if (temp_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            temp_file.write(codeEditor_->toPlainText().toUtf8());
            temp_file.close();
        }
        currentSketchPath_ = temp_path;
        windowTitleBase_ = "VEMCODE — unsaved sketch";
        updateWindowTitle();
    }

    serialMonitor_->clear();
    if (serialMonitor1_) serialMonitor1_->clear();
    if (serialMonitor2_) serialMonitor2_->clear();
    statusBar()->showMessage("Compiling...");
    setRunTriggersEnabled(false);

    QFile file(currentSketchPath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusBar()->showMessage("Failed to write: " + currentSketchPath_);
        setRunTriggersEnabled(true);
        timelineRecorder_.setActive(false);
        return;
    }
    file.write(codeEditor_->toPlainText().toUtf8());
    file.close();

    Compiler compiler;
    compiler.set_compiler_path(compilerPath_.toStdString());
    compiler.set_include_path(projectRoot_.toStdString());

    // Output DLL to same folder as sketch
    std::string sketch_path = currentSketchPath_.toStdString();
    std::string sketch_dir  = sketch_path;
    size_t slash = sketch_dir.find_last_of("/\\");
    if (slash != std::string::npos)
        sketch_dir = sketch_dir.substr(0, slash);
    compiler.set_output_dir(sketch_dir);

    // Static pre-compile checks (pin range, PWM, ISR issues, etc.)
    for (const QString& w : SketchLinter::checkSource(codeEditor_->toPlainText(), activeProfile_))
        serialMonitor_->appendPlainText(w + "\n");

    CompileResult result = compiler.compile(sketch_path);

    // Show pre-compile warnings (unsupported libraries, etc.) regardless of outcome
    for (const auto& w : result.warnings)
        serialMonitor_->appendPlainText(QString::fromStdString(w) + "\n");

    // Apply board hint from // @board comment if present
    if (!result.board_hint.empty()) {
        QString boardName = QString::fromStdString(result.board_hint);
        int oldSerialCount = activeProfile_.serial_count;
        bool knownBoard = boardProfileForName(boardName, activeProfile_);
        if (!knownBoard) {
            serialMonitor_->appendPlainText(
                "WARNING: Unknown board '" + boardName + "' in @board hint — using currently selected board instead.\n");
        }

        if (knownBoard) {
            canvasWidget_->setProfile(activeProfile_);
            boardLabel_->setText(activeProfile_.name);
            if (sketchThread_) sketchThread_->setProfile(activeProfile_);

            if (activeProfile_.serial_count != oldSerialCount)
                rebuildSerialMonitors();

            QSettings settings = appSettings();
            settings.setValue("board/name", boardName);
        }
    }

    if (!result.success) {
        // GCC output references internal temp file paths and the preprocessor's
        // injected header lines -- clean it up before showing it to the user
        QString raw = QString::fromStdString(result.raw_output);

        QString temp_file_path = QString::fromStdString(sketch_dir + "/_vb_temp.cpp");

        QRegularExpression path_re(
            QRegularExpression::escape(temp_file_path + ":") + "(?=(\\d))"
        );
        raw.replace(path_re, "line ");

        QRegularExpression context_re(
            QRegularExpression::escape(temp_file_path + ":")
        );
        raw.replace(context_re, "");

        // Subtract the lines the preprocessor injected above the user's code
        // (Arduino API wrapper, Serial macros, etc.) so errors point to the right line
        QRegularExpression line_num_re(R"(line (\d+):)");
        QString adjusted_raw;
        int last_pos = 0;
        QRegularExpressionMatchIterator it = line_num_re.globalMatch(raw);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            int raw_line = match.captured(1).toInt();
            int adj_line = raw_line - result.header_lines;
            adjusted_raw += raw.mid(last_pos, match.capturedStart() - last_pos);
            adjusted_raw += QString("line %1:").arg(qMax(1, adj_line));
            last_pos = match.capturedEnd();
        }
        adjusted_raw += raw.mid(last_pos);
        raw = adjusted_raw;

        // Strip gcc source context line numbers (e.g. "16 | api->pinMode...")
        QRegularExpression context_line_re(R"(\n\s*\d+\s*\|)");
        raw.replace(context_line_re, "\n   |");

        // Restore Serial.method() names -- preprocessor converted dots to underscores
        raw.replace("Serial_println(",  "Serial.println(");
        raw.replace("Serial_print(",    "Serial.print(");
        raw.replace("Serial_begin(",    "Serial.begin(");
        raw.replace("Serial_available(","Serial.available(");
        raw.replace("Serial_read(",     "Serial.read(");

        // Strip api-> -- user never wrote this
        raw.replace("api->", "");

        raw = SketchLinter::humanizeErrors(raw);
        serialMonitor_->appendPlainText("=== Compile errors ===\n");
        serialMonitor_->appendPlainText(raw);
        statusBar()->showMessage("Compile failed");
        setRunTriggersEnabled(true);
        timelineRecorder_.setActive(false);
        showCompileErrors(result);
        return;
    }

    showCompileErrors(result); // paints warning lines only -- no errors on this path
    signalTimeline_->clear();
    serialPlotter_->clear();
    variableWatch_->clearValues();
    timelineRecorder_.clear();
    simTimer_.start();
    statusBar()->showMessage("Running: " + currentSketchPath_);
    stopButton_->setEnabled(true);
    canvasWidget_->setSketchRunning(true);

    // Reset state and inject component values before starting the sketch --
    // otherwise it can read pins before injection, or see stale prior-run state.
    sketchThread_->stopSketch();
    sketchThread_->resetRuntimeState();

    detector_.detect(codeEditor_->toPlainText().toStdString(), activeProfile_.pin_count - 1);
    canvasWidget_->refresh(detector_.components());
    devicesPanel_->pushAll(); // re-inject the Devices table into the freshly reset runtime
    spiPanel_->pushAll(); // re-inject the SPI response sequence into the freshly reset runtime

    // Show detected components on the serial monitor
    serialMonitor_->appendPlainText(QString::fromStdString("=== Components detected ===\n"));
    for (const auto& comp : detector_.components())
        serialMonitor_->appendPlainText(QString::fromStdString(comp.to_string()));
    serialMonitor_->appendPlainText(QString::fromStdString("===========================\n"));

    // Same-pin-claimed-by-two-components warnings
    for (const auto& w : detector_.warnings())
        serialMonitor_->appendPlainText(QString::fromStdString(w) + "\n");

    // No-components-detected hints
    const auto& comps = detector_.components();
    bool has_non_serial = std::any_of(comps.begin(), comps.end(),
        [](const DetectedComponent& c) { return c.type_name != "Serial"; });
    if (!has_non_serial) {
        std::string src = codeEditor_->toPlainText().toStdString();
        bool has_pin_defines = false;
        std::regex pin_def_re(R"(#\s*define\s+(\w+)\s+\d+)");
        for (auto it = std::sregex_iterator(src.begin(), src.end(), pin_def_re);
             it != std::sregex_iterator(); ++it) {
            if (SketchLinter::isPinName((*it)[1].str())) { has_pin_defines = true; break; }
        }
        bool has_hardcoded = std::regex_search(src,
            std::regex(R"((?:digitalWrite|analogWrite|pinMode)\s*\(\s*\d+)"));
        bool has_local_header = std::regex_search(src,
            std::regex(R"(#include\s+"[^"]+\.h")"));

        if (has_pin_defines) {
            serialMonitor_->appendPlainText(
                "HINT: Pin definitions found but couldn't identify component types — "
                "try descriptive names like LED_PIN, SERVO_PIN, BUTTON_PIN\n");
        } else if (has_hardcoded) {
            serialMonitor_->appendPlainText(
                "HINT: Pin numbers are hardcoded — give them names like "
                "'const int LED_PIN = 5;' so the simulator can identify them\n");
        } else if (has_local_header) {
            serialMonitor_->appendPlainText(
                "HINT: No components detected — if your pin definitions are in a header file, "
                "try moving them into the main sketch\n");
        }
    }

    if (!timelinePlaybackPath_.isEmpty()) {
        QString playbackPath = timelinePlaybackPath_;
        timelinePlaybackPath_.clear(); // one-shot: consumed by this run, not persisted for the next
        try {
            std::vector<TimelineEvent> events = parse_timeline_file(playbackPath.toStdString());
            sketchThread_->armTimeline(detector_.components(), events);
            serialMonitor_->appendPlainText("=== Playing timeline: " + playbackPath + " ===\n");
        } catch (const std::exception& e) {
            statusBar()->showMessage(QString("Failed to load timeline: %1").arg(e.what()));
            setRunTriggersEnabled(true);
            timelineRecorder_.setActive(false);
            return;
        }
    }

    sketchThread_->startSketch(QString::fromStdString(result.dll_path));
}

void MainWindow::onStopClicked() {
    sketchThread_->stopSketch();
    statusBar()->showMessage("Stopped");
    setRunTriggersEnabled(true);
    stopButton_->setEnabled(false);
    canvasWidget_->setSketchRunning(false);
    finishRecordingIfActive();
}

void MainWindow::setRunTriggersEnabled(bool enabled) {
    runButton_->setEnabled(enabled);
    recordButton_->setEnabled(enabled);
    playTimelineButton_->setEnabled(enabled);
}

void MainWindow::finishRecordingIfActive() {
    bool wasRecording = timelineRecorder_.isActive();
    timelineRecorder_.setActive(false);
    if (!wasRecording) return;

    if (timelineRecorder_.isEmpty()) {
        statusBar()->showMessage("Nothing recorded");
        return;
    }

    QString baseName = currentSketchPath_.isEmpty()
        ? "recording" : QFileInfo(currentSketchPath_).completeBaseName();
    QString defaultDir = currentSketchPath_.isEmpty()
        ? defaultSketchLocation_ : QFileInfo(currentSketchPath_).absolutePath();
    QString path = QFileDialog::getSaveFileName(
        this, "Export Timeline", defaultDir + "/" + baseName + ".timeline",
        "Timeline files (*.timeline)"
    );
    if (path.isEmpty()) {
        statusBar()->showMessage("Recording discarded");
        return;
    }

    if (timelineRecorder_.exportToFile(path))
        statusBar()->showMessage("Exported: " + path);
    else
        statusBar()->showMessage("Failed to export: " + path);
}

void MainWindow::onRecordClicked() {
    if (!runButton_->isEnabled()) return; // already running
    timelineRecorder_.setActive(true);
    onRunClicked();
}

void MainWindow::onPlayTimelineClicked() {
    if (!runButton_->isEnabled()) return; // already running

    // Mirrors the headless CLI's timeline=true convention: default to
    // <sketch-name>.timeline next to the sketch if one exists.
    QString defaultPath;
    if (!currentSketchPath_.isEmpty()) {
        QFileInfo info(currentSketchPath_);
        defaultPath = info.absolutePath() + "/" + info.completeBaseName() + ".timeline";
    }

    QString path;
    if (!defaultPath.isEmpty() && QFile::exists(defaultPath)) {
        path = defaultPath;
    } else {
        QString startDir = currentSketchPath_.isEmpty()
            ? defaultSketchLocation_ : QFileInfo(currentSketchPath_).absolutePath();
        path = QFileDialog::getOpenFileName(
            this, "Select Timeline", startDir, "Timeline files (*.timeline)"
        );
    }
    if (path.isEmpty()) return;

    timelinePlaybackPath_ = path;
    onRunClicked();
}

void MainWindow::onLayoutToggled(bool on) {
    canvasWidget_->setLayoutMode(on);
    statusBar()->showMessage(on ? "Layout mode: drag components to reposition" : "Ready");
}

void MainWindow::onOpenClicked() {
    QString sketches_root = defaultSketchLocation_;
    QString path = QFileDialog::getOpenFileName(
        this, "Open sketch", sketches_root, "C++ files (*.cpp *.ino)"
    );
    if (path.isEmpty()) return;
    openSketchFile(path);
}

void MainWindow::openSketchFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        statusBar()->showMessage("Failed to open: " + path);
        return;
    }
    QString contents = QString::fromUtf8(file.readAll());
    file.close();

    currentSketchPath_ = path;
    canvasWidget_->loadLayout(path);
    codeEditor_->setPlainText(contents);
    codeEditor_->document()->setModified(false);

    windowTitleBase_ = "VEMCODE — " + QFileInfo(path).fileName();
    updateWindowTitle();
    checkForAutosaveRecovery(path);
    statusBar()->showMessage("Opened: " + path);
    addToRecentSketches(path);
}

void MainWindow::onSaveClicked() {
    // Silently overwrite the already-open sketch -- but a path under the OS temp dir (the unsaved-scratch-file
    // fallback from onRunClicked) doesn't count as "already open", so that case still prompts for a real name.
    bool has_real_path = !currentSketchPath_.isEmpty()
        && !currentSketchPath_.startsWith(QDir::tempPath());
    if (!has_real_path) {
        promptAndSaveAsNewSketch();
        return;
    }

    QFile file(currentSketchPath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusBar()->showMessage("Failed to save: " + currentSketchPath_);
        return;
    }
    file.write(codeEditor_->toPlainText().toUtf8());
    file.close();
    codeEditor_->document()->setModified(false);
    QFile::remove(currentSketchPath_ + ".autosave");
    canvasWidget_->saveLayout(currentSketchPath_);
    statusBar()->showMessage("Saved: " + currentSketchPath_);
    addToRecentSketches(currentSketchPath_);

    if (autoCompileOnSave_ && runButton_->isEnabled()) onRunClicked();
}

// Always prompts for a name and creates a new sketch, even if one is already open
void MainWindow::onSaveAsClicked() {
    promptAndSaveAsNewSketch();
}

void MainWindow::promptAndSaveAsNewSketch() {
    bool ok;
    QString name = QInputDialog::getText(
        this, "Save sketch", "Sketch name:",
        QLineEdit::Normal, "my_sketch", &ok
    );
    if (!ok || name.trimmed().isEmpty()) return;

    name = name.trimmed().replace(" ", "_");

    QString sketches_root = defaultSketchLocation_;
    QString sketch_dir    = sketches_root + "/" + name;
    QDir().mkpath(sketch_dir);

    QString file_path = sketch_dir + "/" + name + ".cpp";
    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusBar()->showMessage("Failed to save: " + file_path);
        return;
    }
    file.write(codeEditor_->toPlainText().toUtf8());
    file.close();

    // Update current path so Run compiles this file going forward
    currentSketchPath_ = file_path;
    canvasWidget_->saveLayout(file_path);
    codeEditor_->document()->setModified(false);
    QFile::remove(file_path + ".autosave");
    windowTitleBase_ = "VEMCODE — " + name + ".cpp";
    updateWindowTitle();
    statusBar()->showMessage("Saved: " + file_path);
    addToRecentSketches(file_path);

    if (autoCompileOnSave_ && runButton_->isEnabled()) onRunClicked();
}

// If sketchPath has a newer .autosave file sitting next to it (crash recovery),
// offer to load that instead of what was just read from disk
void MainWindow::checkForAutosaveRecovery(const QString& sketchPath) {
    QFileInfo autosaveInfo(sketchPath + ".autosave");
    if (!autosaveInfo.exists()) return;

    QFileInfo sketchInfo(sketchPath);
    if (autosaveInfo.lastModified() <= sketchInfo.lastModified()) return;

    auto reply = QMessageBox::question(this, "Restore autosave?",
        "An autosave from " + autosaveInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss") +
        " is newer than the saved file. Restore it?",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QFile file(autosaveInfo.filePath());
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            codeEditor_->setPlainText(QString::fromUtf8(file.readAll()));
            file.close();
            codeEditor_->document()->setModified(true);
        }
    } else {
        QFile::remove(autosaveInfo.filePath());
    }
}

// Window title reflects windowTitleBase_ plus a "*" while the editor has unsaved changes
void MainWindow::updateWindowTitle() {
    bool modified = codeEditor_ && codeEditor_->document()->isModified();
    setWindowTitle(windowTitleBase_ + (modified ? " *" : ""));
}

void MainWindow::adjustEditorZoom(int steps) {
    editorZoomLevel_ = qBound(-8, editorZoomLevel_ + steps, 20);
    QFont f = codeEditor_->font();
    f.setPointSize(qMax(6, DEFAULT_EDITOR_FONT_SIZE + editorZoomLevel_));
    codeEditor_->setFont(f);
}

void MainWindow::resetEditorZoom() {
    editorZoomLevel_ = 0;
    QFont f = codeEditor_->font();
    f.setPointSize(DEFAULT_EDITOR_FONT_SIZE);
    codeEditor_->setFont(f);
}

void MainWindow::refreshExtraSelections() {
    codeEditor_->setExtraSelections(compileSelections_ + bracketSelections_ + findSelections_);
}

// Highlights the bracket pair adjacent to the cursor, if any. Checks the character
// just after the cursor first, then just before, matching common editor convention.
void MainWindow::updateBracketMatch() {
    bracketSelections_.clear();

    static const QString kOpeners = "({[";
    static const QString kClosers = ")}]";

    QTextDocument* doc = codeEditor_->document();
    int pos = codeEditor_->textCursor().position();

    int bracketPos = -1;
    QChar c;
    if (!doc->characterAt(pos).isNull() &&
        (kOpeners.contains(doc->characterAt(pos)) || kClosers.contains(doc->characterAt(pos)))) {
        bracketPos = pos;
        c = doc->characterAt(pos);
    } else if (pos > 0 &&
               (kOpeners.contains(doc->characterAt(pos - 1)) || kClosers.contains(doc->characterAt(pos - 1)))) {
        bracketPos = pos - 1;
        c = doc->characterAt(pos - 1);
    }

    if (bracketPos >= 0) {
        bool isOpen = kOpeners.contains(c);
        QChar match = isOpen ? kClosers[kOpeners.indexOf(c)] : kOpeners[kClosers.indexOf(c)];
        int direction = isOpen ? 1 : -1;

        int depth = 0;
        int i = bracketPos;
        int matchPos = -1;
        while (true) {
            QChar ch = doc->characterAt(i);
            if (ch.isNull()) break;
            if (ch == c) depth++;
            else if (ch == match) {
                depth--;
                if (depth == 0) { matchPos = i; break; }
            }
            i += direction;
        }

        if (matchPos >= 0) {
            for (int p : {bracketPos, matchPos}) {
                QTextEdit::ExtraSelection sel;
                sel.format.setBackground(highlightColors_.bracket_match_bg);
                sel.cursor = QTextCursor(doc);
                sel.cursor.setPosition(p);
                sel.cursor.setPosition(p + 1, QTextCursor::KeepAnchor);
                bracketSelections_.append(sel);
            }
        }
    }

    refreshExtraSelections();
}

// Editor helpers
bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (completer_ && obj == completer_->popup() && event->type() == QEvent::KeyPress) {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        switch (key->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Tab: {
            QModelIndex idx = completer_->popup()->currentIndex();
            if (idx.isValid())
                insertCompletion(idx.data().toString());
            completer_->popup()->hide();
            return true;
        }
        case Qt::Key_Escape:
            completer_->popup()->hide();
            return true;
        default:
            break;
        }
    }

    if (obj == speedSlider_ && event->type() == QEvent::MouseButtonDblClick) {
        speedSlider_->setValue(13); // 1.0x -- see speedForSliderValue()
        return true;
    }

    if (obj == codeEditor_ && event->type() == QEvent::Wheel) {
        QWheelEvent* wheel = static_cast<QWheelEvent*>(event);
        if (wheel->modifiers() & Qt::ControlModifier) {
            adjustEditorZoom(wheel->angleDelta().y() > 0 ? 1 : -1);
            return true;
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::showCompletionPopup() {
    static const QStringList kCompletionWords = {
        // GPIO / timing / interrupts
        "pinMode", "digitalWrite", "digitalRead", "analogWrite", "analogRead",
        "delay", "delayMicroseconds", "millis", "micros", "tone", "noTone",
        "attachInterrupt", "detachInterrupt", "noInterrupts", "interrupts",
        "digitalPinToInterrupt",
        // Serial
        "Serial.begin", "Serial.print", "Serial.println", "Serial.available", "Serial.read",
        "Serial1.begin", "Serial1.print", "Serial1.println",
        "Serial2.begin", "Serial2.print", "Serial2.println",
        // EEPROM
        "EEPROM.write", "EEPROM.read", "EEPROM.update",
        // Watchdog / sleep
        "wdt_enable", "wdt_disable", "wdt_reset",
        "set_sleep_mode", "sleep_enable", "sleep_disable", "sleep_cpu", "sleep_mode",
        "WDTO_15MS", "WDTO_30MS", "WDTO_60MS", "WDTO_120MS", "WDTO_250MS", "WDTO_500MS",
        "WDTO_1S", "WDTO_2S", "WDTO_4S", "WDTO_8S",
        "SLEEP_MODE_IDLE", "SLEEP_MODE_ADC", "SLEEP_MODE_PWR_DOWN",
        "SLEEP_MODE_PWR_SAVE", "SLEEP_MODE_STANDBY", "SLEEP_MODE_EXT_STANDBY",
        // Wire (I2C)
        "Wire.begin", "Wire.beginTransmission", "Wire.write", "Wire.endTransmission",
        "Wire.requestFrom", "Wire.available", "Wire.read",
        // SPI
        "SPI.begin", "SPI.transfer", "SPI.beginTransaction", "SPI.endTransaction",
        "SPISettings", "MSBFIRST", "LSBFIRST", "SPI_MODE0", "SPI_MODE1", "SPI_MODE2", "SPI_MODE3",
        // Servo
        "Servo", "attach", "write", "read", "detach", "attached",
        // LiquidCrystal
        "LiquidCrystal", "lcd.begin", "lcd.print", "lcd.setCursor", "lcd.clear",
        // SoftwareSerial
        "SoftwareSerial",
        // AVR direct registers
        "DDRB", "DDRC", "DDRD", "PORTB", "PORTC", "PORTD", "PINB", "PINC", "PIND",
        "TCCR1A", "TCCR1B", "TCNT1", "OCR1A", "OCR1B", "TIMSK1",
        "TCCR2A", "TCCR2B", "TCNT2", "OCR2A", "OCR2B", "TIMSK2",
        "ISR",
        // Pure computation (no hardware I/O, not rewritten by the preprocessor)
        "map", "constrain", "abs", "min", "max", "random", "randomSeed",
        "pulseIn", "shiftIn", "shiftOut",
        // Constants
        "HIGH", "LOW", "INPUT", "OUTPUT", "INPUT_PULLUP",
        "LED_BUILTIN", "A0", "A1", "A2", "A3", "A4", "A5",
        "CHANGE", "RISING", "FALLING",
        "PI", "TWO_PI", "HALF_PI",
        "true", "false",
        // Types / declarations
        "void", "int", "unsigned int", "long", "unsigned long", "float", "double",
        "char", "byte", "bool", "boolean", "String", "const", "static", "volatile",
        // Sketch skeleton
        "setup", "loop",
        // Control flow
        "if", "else", "for", "while", "do", "switch", "case", "break", "continue", "return",
        // Preprocessor directives -- WordUnderCursor doesn't include the '#',
        // so these match against the text typed after it (e.g. "def" -> "define")
        "define", "include", "ifdef", "ifndef", "endif", "pragma",
    };

    QStringList symbols = SketchLinter::scanSymbols(codeEditor_->toPlainText());
    symbols += kCompletionWords;

    completer_->setModel(new QStringListModel(symbols, completer_));

    QTextCursor tc = codeEditor_->textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    completer_->setCompletionPrefix(tc.selectedText());
    completer_->popup()->setCurrentIndex(completer_->completionModel()->index(0, 0));

    QRect cr = codeEditor_->cursorRect();
    cr.setWidth(completer_->popup()->sizeHintForColumn(0)
                + completer_->popup()->verticalScrollBar()->sizeHint().width());
    completer_->complete(cr);
}

void MainWindow::showMemberCompletionPopup() {
    // Only globals/objects VEMCODE actually implements (Preprocessor::replace_api_calls, src/core/build/libs/*.inc),
    // so this never suggests a real-Arduino method that would silently no-op here.
    static const QStringList kFixedGlobals = {
        "Serial", "Serial1", "Serial2", "Wire", "SPI", "EEPROM"
    };
    static const QMap<QString, QStringList> kMemberTables = {
        {"Serial",         {"begin", "print", "println", "printf", "available", "read"}},
        {"Serial1",        {"begin", "print", "println"}},
        {"Serial2",        {"begin", "print", "println"}},
        {"Wire",           {"begin", "beginTransmission", "write", "endTransmission",
                             "requestFrom", "available", "read"}},
        {"SPI",            {"begin", "beginTransaction", "endTransaction", "transfer"}},
        {"EEPROM",         {"read", "write", "update"}},
        {"Servo",          {"attach", "write", "read", "attached", "detach"}},
        {"LiquidCrystal",  {"begin", "clear", "setCursor", "write", "print", "createChar"}},
        {"SoftwareSerial", {"begin", "print", "println", "available", "read", "peek",
                             "write", "listen", "isListening", "overflow"}},
    };

    QTextCursor tc = codeEditor_->textCursor();
    QString before = tc.block().text().left(tc.positionInBlock());
    static const QRegularExpression receiverRe(R"(\b(\w+)\.$)");
    QRegularExpressionMatch m = receiverRe.match(before);
    if (!m.hasMatch())
        return;

    QString receiver = m.captured(1);
    QString type = kFixedGlobals.contains(receiver)
        ? receiver
        : SketchLinter::scanDeclaredTypes(codeEditor_->toPlainText()).value(receiver);
    if (!kMemberTables.contains(type))
        return;

    completer_->setModel(new QStringListModel(kMemberTables.value(type), completer_));
    completer_->setCompletionPrefix(QString());
    completer_->popup()->setCurrentIndex(completer_->completionModel()->index(0, 0));

    QRect cr = codeEditor_->cursorRect();
    cr.setWidth(completer_->popup()->sizeHintForColumn(0)
                + completer_->popup()->verticalScrollBar()->sizeHint().width());
    completer_->complete(cr);
}

void MainWindow::clearCompileErrors() {
    compileSelections_.clear();
    refreshExtraSelections();
}

void MainWindow::showCompileErrors(const CompileResult& result) {
    compileSelections_.clear();

    for (const auto& err : result.errors) {
        int adjusted_line = err.line - result.header_lines;
        if (adjusted_line < 1) continue;
        QTextBlock block = codeEditor_->document()->findBlockByLineNumber(
            adjusted_line - 1);
        if (!block.isValid()) continue;

        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(err.is_error ? highlightColors_.error_bg : highlightColors_.warning_bg);
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.format.setToolTip(QString::fromStdString(err.message));
        sel.cursor = QTextCursor(block);
        sel.cursor.select(QTextCursor::LineUnderCursor);
        compileSelections_.append(sel);
    }

    refreshExtraSelections();

    for (const auto& err : result.errors) {
        if (err.is_error) {
            int adjusted_line = err.line - result.header_lines;
            if (adjusted_line < 1) continue;
            QTextBlock block = codeEditor_->document()
                ->findBlockByLineNumber(adjusted_line - 1);
            if (block.isValid()) {
                codeEditor_->setTextCursor(QTextCursor(block));
                codeEditor_->centerCursor();
                break;
            }
        }
    }
}

void MainWindow::setAppTheme(bool dark) {
    darkTheme_ = dark;
    qApp->setPalette(appQPalette(dark));
    qApp->setStyleSheet(appStylesheet(dark));
    highlightColors_ = editorHighlightColors(dark);
    canvasWidget_->setDarkTheme(dark);
    if (highlighter_) highlighter_->setTheme(dark);
    if (signalTimeline_) signalTimeline_->setDarkTheme(dark);
    if (serialPlotter_) serialPlotter_->setDarkTheme(dark);
    if (lineNumbers_) lineNumbers_->setDarkTheme(dark);
    if (codeEditor_) codeEditor_->setDarkTheme(dark);
    if (findReplaceBar_) findReplaceBar_->setHighlightColor(highlightColors_.find_match_bg);
}

void MainWindow::onSettingsClicked() {
    QSettings settings = appSettings();

    SettingsDialog dialog(this);
    dialog.setCompilerPath(compilerPath_);
    dialog.setProjectRoot(projectRoot_);
    dialog.setSelectedBoard(QString(activeProfile_.name));
    dialog.setAnalogNoise(analogNoise_);
    dialog.setAutoCompileOnSave(autoCompileOnSave_);
    dialog.setDarkTheme(darkTheme_);
    dialog.setDefaultSketchLocation(defaultSketchLocation_);
    dialog.setKeybinds(keybinds_.all());

    if (dialog.exec() == QDialog::Accepted) {
        compilerPath_      = dialog.compilerPath();
        projectRoot_       = dialog.projectRoot();
        analogNoise_       = dialog.analogNoise();
        autoCompileOnSave_ = dialog.autoCompileOnSave();
        defaultSketchLocation_ = dialog.defaultSketchLocation();
        settings.setValue("compiler/path", compilerPath_);
        settings.setValue("compiler/project_root", projectRoot_);
        settings.setValue("sketches/default_location", defaultSketchLocation_);
        settings.setValue("simulation/analog_noise", analogNoise_);
        settings.setValue("editor/auto_compile_on_save", autoCompileOnSave_);
        darkTheme_ = dialog.darkTheme();
        settings.setValue("canvas/dark_theme", darkTheme_);
        setAppTheme(darkTheme_);
        keybinds_.apply(dialog.keybinds());
        codeEditor_->setActionKeybinds(keybinds_.value("code_completion"),
                                        keybinds_.value("duplicate_line"),
                                        keybinds_.value("comment_toggle"));
        applyBoardChange(dialog.selectedBoard());
        if (sketchThread_) sketchThread_->setAnalogNoise(analogNoise_);
        statusBar()->showMessage("Settings saved");
    }
}

void MainWindow::applyBoardChange(const BoardProfile& profile) {
    int oldSerialCount = activeProfile_.serial_count;
    activeProfile_ = profile;

    QSettings settings = appSettings();
    settings.setValue("board/name", QString(activeProfile_.name));

    canvasWidget_->setProfile(activeProfile_);
    boardLabel_->setText(activeProfile_.name);
    if (sketchThread_) sketchThread_->setProfile(activeProfile_);
    if (activeProfile_.serial_count != oldSerialCount)
        rebuildSerialMonitors();
}

// Shared tail of onNewSketch()/onOpenExample() -- writes <name>/<name>.cpp
// under the configured save location and loads it as the current sketch.
void MainWindow::createSketchFromContent(const QString& name, const QString& content) {
    QString sketch_dir = defaultSketchLocation_ + "/" + name;
    QDir().mkpath(sketch_dir);

    QString file_path = sketch_dir + "/" + name + ".cpp";
    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusBar()->showMessage("Failed to create: " + file_path);
        return;
    }
    file.write(content.toUtf8());
    file.close();

    codeEditor_->setPlainText(content);
    codeEditor_->document()->setModified(false);
    currentSketchPath_ = file_path;
    canvasWidget_->loadLayout(file_path);
    windowTitleBase_ = "VEMCODE — " + name + ".cpp";
    updateWindowTitle();
    statusBar()->showMessage("Created: " + file_path);
    addToRecentSketches(file_path);
}

void MainWindow::onNewSketch() {
    static const QString kBlankSketch =
        "#define LED_PIN 13\n\n"
        "void setup() {\n"
        "    Serial.begin(9600);\n"
        "}\n\n"
        "void loop() {\n"
        "}\n";

    // Shipped starters live next to the executable, not the user's (possibly relocated) default save folder.
    QString templatesDir = QCoreApplication::applicationDirPath() + "/sketches/templates";
    QList<SketchManifestEntry> templates = loadSketchManifest(templatesDir + "/manifest.json");

    QDialog dialog(this);
    dialog.setWindowTitle("New Sketch");
    auto* layout = new QVBoxLayout(&dialog);

    layout->addWidget(new QLabel("Sketch name:", &dialog));
    auto* nameEdit = new QLineEdit("my_sketch", &dialog);
    layout->addWidget(nameEdit);

    layout->addWidget(new QLabel("Start from:", &dialog));
    auto* list = new QListWidget(&dialog);
    list->addItem("Blank");
    for (const auto& t : templates) {
        auto* item = new QListWidgetItem(t.name, list);
        item->setToolTip(t.description);
    }
    list->setCurrentRow(0);
    layout->addWidget(list);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) return;

    QString name = nameEdit->text().trimmed().replace(" ", "_");
    if (name.isEmpty()) return;

    int row = list->currentRow();
    QString content = kBlankSketch;
    if (row > 0 && row - 1 < templates.size()) {
        const auto& t = templates[row - 1];
        QFile f(templatesDir + "/" + t.folder + "/" + t.file);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            content = QString::fromUtf8(f.readAll());
    }

    createSketchFromContent(name, content);
}

void MainWindow::onOpenExample() {
    QString examplesDir = QCoreApplication::applicationDirPath() + "/sketches/examples";
    QList<SketchManifestEntry> examples = loadSketchManifest(examplesDir + "/manifest.json");
    if (examples.isEmpty()) {
        statusBar()->showMessage("No examples found.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Open Example");
    dialog.resize(420, 360);
    auto* layout = new QVBoxLayout(&dialog);

    auto* tree = new QTreeWidget(&dialog);
    tree->setHeaderHidden(true);
    QMap<QString, QTreeWidgetItem*> categories;
    for (const auto& ex : examples) {
        QString category = ex.category.isEmpty() ? "Other" : ex.category;
        QTreeWidgetItem*& catItem = categories[category];
        if (!catItem) catItem = new QTreeWidgetItem(tree, {category});
        auto* item = new QTreeWidgetItem(catItem, {ex.name});
        item->setToolTip(0, ex.description);
        item->setData(0, Qt::UserRole, ex.folder + "|" + ex.file);
    }
    tree->expandAll();
    layout->addWidget(tree);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Open)->setEnabled(false);
    connect(tree, &QTreeWidget::itemSelectionChanged, &dialog, [tree, buttons]() {
        auto items = tree->selectedItems();
        bool leaf = !items.isEmpty() && items.first()->childCount() == 0 && items.first()->parent();
        buttons->button(QDialogButtonBox::Open)->setEnabled(leaf);
    });
    connect(tree, &QTreeWidget::itemDoubleClicked, &dialog, [&dialog](QTreeWidgetItem* item, int) {
        if (item->childCount() == 0 && item->parent()) dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) return;

    auto selected = tree->selectedItems();
    if (selected.isEmpty()) return;
    QTreeWidgetItem* item = selected.first();
    if (item->childCount() > 0 || !item->parent()) return;

    QStringList parts = item->data(0, Qt::UserRole).toString().split("|");
    if (parts.size() != 2) return;

    QFile f(examplesDir + "/" + parts[0] + "/" + parts[1]);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        statusBar()->showMessage("Failed to open example.");
        return;
    }
    QString content = QString::fromUtf8(f.readAll());

    bool ok;
    QString name = QInputDialog::getText(
        this, "New sketch from example", "Sketch name:",
        QLineEdit::Normal, item->text(0).replace(" ", "_"), &ok
    );
    if (!ok || name.trimmed().isEmpty()) return;

    createSketchFromContent(name.trimmed().replace(" ", "_"), content);
}

void MainWindow::populateRecentMenu(QMenu* menu) {
    menu->clear();

    QSettings settings = appSettings();
    QStringList recent = settings.value("recent/sketches").toStringList();

    if (recent.isEmpty()) {
        QAction* empty = menu->addAction("No recent sketches");
        empty->setEnabled(false);
        return;
    }

    for (const QString& path : recent) {
        QAction* action = menu->addAction(QFileInfo(path).fileName());
        action->setToolTip(path);
        connect(action, &QAction::triggered, this, [this, path]() {
            if (!QFile::exists(path)) {
                statusBar()->showMessage("File not found: " + path);
                return;
            }
            openSketchFile(path);
        });
    }
}

void MainWindow::addToRecentSketches(const QString& path) {
    QSettings settings = appSettings();
    QStringList recent = settings.value("recent/sketches").toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 5)
        recent.removeLast();
    settings.setValue("recent/sketches", recent);
}

// Two log-scaled segments joined at 1.0x (tick 13): 1..13 spans 0.1x-1.0x, 13..25 spans 1.0x-5.0x, so equal
// slider distance means equal *ratio* change in speed, matching how a multiplier is actually perceived.
static float speedForSliderValue(int value) {
    if (value <= 13)
        return 0.1f * std::pow(10.0f, (value - 1) / 12.0f);
    return std::pow(5.0f, (value - 13) / 12.0f);
}

void MainWindow::onSpeedChanged(int value) {
    float display_speed = speedForSliderValue(value);
    sketchThread_->setSpeed(display_speed);
    speedSlider_->setToolTip(QString("Speed: %1x").arg(display_speed, 0, 'f', 2));
    QToolTip::showText(QCursor::pos(), speedSlider_->toolTip(), speedSlider_);
}

void MainWindow::onSerialSend() {
    QString text = serialInput_->text();
    if (text.isEmpty()) return;
    if (timelineRecorder_.isActive())
        timelineRecorder_.recordSerialSend(text, simTimer_.elapsed() / 1000.0);
    sketchThread_->injectSerial(text + "\n");
    serialMonitor_->appendPlainText("> " + text);
    serialInput_->clear();
}

void MainWindow::showDocViewer(const QString& title, const QString& filename) {
    QString path = QString(projectRoot_) + "/docs/" + filename;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        statusBar()->showMessage("Doc not found: " + path);
        return;
    }
    QString markdown = QString::fromUtf8(file.readAll());
    file.close();

    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(title);
    dialog->resize(800, 600);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(0, 0, 0, 0);

    QTextBrowser* browser = new QTextBrowser(dialog);
    browser->setOpenExternalLinks(true); // so links *within* the doc still work normally
    browser->setMarkdown(markdown);
    layout->addWidget(browser);

    dialog->show();
}