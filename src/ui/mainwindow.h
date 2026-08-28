#pragma once
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QTabWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QFile>
#include <QElapsedTimer>
#include <QDir>
#include <QShortcut>
#include <QSettings>
#include <QSlider>
#include <QLineEdit>
#include <QCompleter>
#include <QStringListModel>
#include <QList>
#include <QTimer>
#include <QScrollBar>
#include <QCloseEvent>
#include <qcombobox.h>
#include "src/core/host/sketchhostthread.h"
#include "src/core/host/timeline.h"
#include "src/core/build/compiler.h"
#include "src/ui/canvaswidget.h"
#include "src/core/circuit/circuitdetector.h"
#include "src/ui/panels/signaltimeline.h"
#include "src/ui/panels/serialplotter.h"
#include "src/ui/editor/codehighlighter.h"
#include "src/core/build/preprocessor.h"
#include "src/ui/editor/linenumberarea.h"
#include "src/ui/panels/variablewatch.h"
#include "src/ui/panels/devicespanel.h"
#include "src/ui/panels/spipanel.h"
#include "src/ui/settingsdialog.h"
#include "src/ui/panels/addcomponentdialog.h"
#include "src/ui/apptheme.h"
#include "src/core/runtime/boardprofile.h"
#include "src/ui/editor/keybindmanager.h"
#include "src/ui/editor/findreplacebar.h"
#include "src/ui/timelinerecorder.h"

// One entry per debug-panel tab, tracked so the Window menu can show/hide
// tabs individually while preserving their relative order on re-insert.
struct DebugTabEntry {
    QString  label;
    QWidget* widget = nullptr;
    QString  settingsKey;
    QAction* action = nullptr;  // set once setupMenuBar() builds the Window menu
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    // Loads a sketch by path the same way onOpenClicked's file dialog does --
    // used both there and by main()'s `ui=true` startup flag.
    void openSketchFile(const QString& path);

    SignalTimeline* signalTimeline_ = nullptr;
    QElapsedTimer   simTimer_;
    VariableWatch* variableWatch_ = nullptr;
    DevicesPanel*  devicesPanel_  = nullptr;
    SpiPanel*      spiPanel_      = nullptr;
    SerialPlotter* serialPlotter_ = nullptr;

private slots:
    void onSerialOutput(QString text);
    void onSerial1Output(QString text);
    void onSerial2Output(QString text);
    void onSoftSerialOutput(int rxPin, QString text);
    void onPinChanged(int pin, int value);
    void onComponentInput(int pin, int eventType, QVariant value);
    void onSketchReloaded();
    void onLoadFailed(QString reason);
    void onRunClicked();
    void onStopClicked();
    void onOpenClicked();
    void onSettingsClicked();
    void onSaveClicked();
    void onSaveAsClicked();
    void onNewSketch();
    void onOpenExample();
    void addToRecentSketches(const QString& path);
    void onSpeedChanged(int value);
    void onSerialSend();
    void insertCompletion(const QString& completion);
    void onLayoutToggled(bool on);
    void onRecordClicked();
    void onPlayTimelineClicked();

private:
    void setupToolbar(QWidget* parent, QVBoxLayout* layout);
    void setupMenuBar();
    void setupMainArea(QWidget* parent, QVBoxLayout* layout);
    bool eventFilter(QObject* obj, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    
    void showDocViewer(const QString& title, const QString& filename);
    // Creates <defaultSketchLocation_>/<name>/<name>.cpp with the given starting
    // content and loads it, the shared tail of onNewSketch()/onOpenExample().
    void createSketchFromContent(const QString& name, const QString& content);

    void showCompileErrors(const CompileResult& result);
    void clearCompileErrors();
    void showCompletionPopup();
    void showMemberCompletionPopup();
    void updateWindowTitle();
    void adjustEditorZoom(int steps);
    void resetEditorZoom();
    void promptAndSaveAsNewSketch();
    void refreshExtraSelections();
    void updateBracketMatch();
    void checkForAutosaveRecovery(const QString& sketchPath);

    QWidget* buildEditorPanel();
    QWidget* buildCanvasPanel();
    QWidget* buildDebugPanel();
    QWidget* buildSerialPanel();
    void     rebuildSerialMonitors();

    // Switches activeProfile_, persists it, and pushes it to the canvas/
    // toolbar/sketch thread. Shared by the Settings dialog and the Board menu.
    void applyBoardChange(const BoardProfile& profile);

    // Shows/hides a debug tab and persists the choice; insert position derives from debugTabToggles_ order.
    void setDebugTabVisible(DebugTabEntry& entry, bool visible);
    int  debugTabInsertIndex(const DebugTabEntry& entry) const;

    // Re-checks every Window menu toggle (routes through the same QActions users click) and resets splitter sizes.
    void resetWindowLayout();

    void populateRecentMenu(QMenu* menu);

    // Mirrors enabled state across Run/Record/Play Timeline -- stopButton_ has its own timing, not touched here.
    void setRunTriggersEnabled(bool enabled);

    // Ends a Record-triggered run: prompts for a save path and exports, or just a status message if nothing was captured.
    void finishRecordingIfActive();

    // Toolbar
    QPushButton*    runButton_      = nullptr;
    QPushButton*    stopButton_     = nullptr;
    QPushButton*    recordButton_   = nullptr;
    QPushButton*    playTimelineButton_ = nullptr;
    QLabel*         boardLabel_     = nullptr;
    QPushButton*    layoutButton_   = nullptr;

    // Splitters -- top-level (editor | rest) and right side (canvas / debug)
    QSplitter* mainSplitter_  = nullptr;
    QSplitter* rightSplitter_ = nullptr;

    // Window menu panel-visibility actions, kept so resetWindowLayout() can
    // re-check them (and let their own toggled() handler do the work).
    QAction* editorPanelAction_ = nullptr;
    QAction* canvasPanelAction_ = nullptr;
    QAction* debugPanelAction_  = nullptr;

    // Editor panel (left)
    QWidget* editorPanel_ = nullptr;
    EditorWithLines* codeEditor_  = nullptr;
    CodeHighlighter* highlighter_ = nullptr;
    LineNumberArea* lineNumbers_  = nullptr;
    QCompleter* completer_        = nullptr;
    QTimer*     idleCompletionTimer_ = nullptr;
    QTimer*     autosaveTimer_       = nullptr;

    // Editor extra-selection layers -- merged together in refreshExtraSelections()
    // since QPlainTextEdit::setExtraSelections() replaces the whole list each call
    QList<QTextEdit::ExtraSelection> compileSelections_;
    QList<QTextEdit::ExtraSelection> bracketSelections_;
    QList<QTextEdit::ExtraSelection> findSelections_;

    // Find & Replace bar
    FindReplaceBar* findReplaceBar_ = nullptr;

    // Canvas panel (top right)
    QWidget*        canvasPanel_    = nullptr;
    CanvasWidget*   canvasWidget_   = nullptr;

    // Debug panel (bottom right)
    QWidget*        debugPanel_      = nullptr;
    QTabWidget*     debugTabs_       = nullptr;
    QPlainTextEdit* serialMonitor_   = nullptr;
    QPlainTextEdit* serialMonitor1_  = nullptr;
    QPlainTextEdit* serialMonitor2_  = nullptr;
    QList<DebugTabEntry> debugTabToggles_;

    // Simulation
    SketchThread*      sketchThread_  = nullptr;
    QString            currentSketchPath_;
    TimelineRecorder   timelineRecorder_;
    QString            timelinePlaybackPath_;

    // Window title base (sans "unsaved changes" marker) and editor zoom level
    QString windowTitleBase_ = "VEMCODE";
    int     editorZoomLevel_ = 0;

    // Circuit detection
    CircuitDetector    detector_;

    // Settings
    QString compilerPath_;
    QString projectRoot_;
    QString defaultSketchLocation_;
    BoardProfile activeProfile_ = BOARD_UNO;
    bool analogNoise_ = false;
    bool autoCompileOnSave_ = false;
    bool darkTheme_ = true;
    EditorHighlightColors highlightColors_ = editorHighlightColors(true);
    void setAppTheme(bool dark); // qApp stylesheet + canvas + syntax highlighter + signal timeline

    // Falls back to defaultKeybinds() (settingsdialog.h) for any id with no
    // saved override yet.
    KeybindManager keybinds_;

    QSlider*   speedSlider_  = nullptr;
    QLineEdit* serialInput_  = nullptr;

    // Per-port line-start tracking for SoftwareSerial prefix insertion
    QMap<int, bool> softSerialLineStart_;
};