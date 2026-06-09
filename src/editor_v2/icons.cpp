#include "icons.h"

static fa::QtAwesome* _icons = nullptr;

fa::QtAwesome* qta()
{
    if (!_icons) {
        _icons = new fa::QtAwesome();
        _icons->initFontAwesome();
    }
    return _icons;
}
