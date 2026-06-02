#include <QApplication>
#include <QComboBox>
#include <QEvent>

#include "MainWindow.h"

// Block wheel events on QComboBox so scrolling never changes the selected value.
class NoScrollComboFilter : public QObject {
public:
    explicit NoScrollComboFilter(QObject* parent = nullptr) : QObject(parent) {}
protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::Wheel && qobject_cast<QComboBox*>(obj))
            return true;  // consume — do not pass to the combobox
        return QObject::eventFilter(obj, event);
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("obs-graphics-editor");
    app.setOrganizationName("obs-graphics");

    app.installEventFilter(new NoScrollComboFilter(&app));

    MainWindow w;
    w.show();

    return app.exec();
}
