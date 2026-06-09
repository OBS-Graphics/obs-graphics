#include "SceneDocument.h"

#include <fstream>
#include <stdexcept>

#include <QFileInfo>

using json = nlohmann::json;

// ── helpers ───────────────────────────────────────────────────────────────────

static std::string easingToString(Easing e)
{
    switch (e) {
    case Easing::EaseIn:
        return "ease_in";
    case Easing::EaseOut:
        return "ease_out";
    case Easing::EaseInOut:
        return "ease_in_out";
    default:
        return "linear";
    }
}

static std::string animTypeToString(AnimationType t)
{
    switch (t) {
    case AnimationType::Fade:
        return "fade";
    case AnimationType::SlideUp:
        return "slide_up";
    case AnimationType::SlideDown:
        return "slide_down";
    case AnimationType::SlideLeft:
        return "slide_left";
    case AnimationType::SlideRight:
        return "slide_right";
    case AnimationType::ScaleIn:
        return "scale_in";
    case AnimationType::WipeUp:
        return "wipe_up";
    case AnimationType::WipeDown:
        return "wipe_down";
    case AnimationType::WipeLeft:
        return "wipe_left";
    case AnimationType::WipeRight:
        return "wipe_right";
    default:
        return "none";
    }
}

static std::string fontWeightToString(FontWeight w)
{
    // Map by underlying Pango weight value
    switch (static_cast<int>(w)) {
    case PANGO_WEIGHT_THIN:
        return "thin";
    // UltraThin shares the same Pango value as Thin; output "thin"
    case PANGO_WEIGHT_ULTRALIGHT:
        return "ultralight";
    case PANGO_WEIGHT_SEMILIGHT:
        return "semilight";
    case PANGO_WEIGHT_LIGHT:
        return "light";
    case PANGO_WEIGHT_BOOK:
        return "book";
    case PANGO_WEIGHT_MEDIUM:
        return "medium";
    case PANGO_WEIGHT_SEMIBOLD:
        return "semibold";
    case PANGO_WEIGHT_BOLD:
        return "bold";
    case PANGO_WEIGHT_ULTRABOLD:
        return "ultrabold";
    case PANGO_WEIGHT_HEAVY:
        return "heavy";
    case PANGO_WEIGHT_ULTRAHEAVY:
        return "ultraheavy";
    default:
        return "normal";
    }
}

static std::string alignmentHToString(HorizontalAlignment a)
{
    switch (a) {
    case HorizontalAlignment::Justify:
        return "justify";
    case HorizontalAlignment::Center:
        return "center";
    case HorizontalAlignment::Right:
        return "right";
    default:
        return "left";
    }
}

static std::string alignmentVToString(VerticalAlignment a)
{
    switch (a) {
    case VerticalAlignment::Bottom:
        return "bottom";
    case VerticalAlignment::Middle:
        return "middle";
    default:
        return "top";
    }
}

static std::string ellipsizeToString(Ellipsize e)
{
    switch (e) {
    case Ellipsize::Start:
        return "start";
    case Ellipsize::Middle:
        return "middle";
    case Ellipsize::End:
        return "end";
    default:
        return "none";
    }
}

static std::string wrapModeToString(WrapMode w)
{
    switch (w) {
    case WrapMode::Char:
        return "char";
    case WrapMode::WordChar:
        return "word_char";
    default:
        return "word";
    }
}

static std::string textTransformToString(TextTransform t)
{
    switch (t) {
    case TextTransform::Capitalize:
        return "capitalize";
    case TextTransform::Uppercase:
        return "uppercase";
    case TextTransform::Lowercase:
        return "lowercase";
    default:
        return "none";
    }
}

static json paintToJson(const Paint& p)
{
    if (p.type == Paint::Type::None)
        return json(nullptr);

    if (p.type == Paint::Type::Solid) {
        return json::array({p.params[0], p.params[1], p.params[2], p.params[3]});
    }

    json obj;
    if (p.type == Paint::Type::Linear) {
        obj["type"] = "linear";
        obj["x0"] = p.params[0];
        obj["y0"] = p.params[1];
        obj["x1"] = p.params[2];
        obj["y1"] = p.params[3];
    } else { // Radial
        obj["type"] = "radial";
        obj["cx0"] = p.params[0];
        obj["cy0"] = p.params[1];
        obj["r0"] = p.params[2];
        obj["cx1"] = p.params[3];
        obj["cy1"] = p.params[4];
        obj["r1"] = p.params[5];
    }

    json stops = json::array();
    for (const auto& s : p.stops) {
        json stop;
        stop["offset"] = s.offset;
        stop["color"] = json::array({s.r, s.g, s.b, s.a});
        stops.push_back(stop);
    }
    obj["stops"] = stops;
    return obj;
}

static json animDefToJson(const AnimationDef& a)
{
    return {{"type", animTypeToString(a.type)},
            {"easing", easingToString(a.easing)},
            {"duration", a.duration},
            {"delay", a.delay}};
}

static json elementToJson(const Element& el)
{
    json j;
    j["id"] = el.id;
    j["type"] = (el.type == ElementType::Text) ? "text" : "rectangle";
    j["x"] = el.bounds.x;
    j["y"] = el.bounds.y;
    j["w"] = el.bounds.width;
    j["h"] = el.bounds.height;

    j["z_order"] = el.zOrder;
    j["opacity"] = el.opacity;
    j["rotation"] = el.rotation;
    j["stroke_width"] = el.strokeWidth;

    // Corner radius: emit as number if all equal, else array
    bool allEqual = el.cornerRadius[0] == el.cornerRadius[1] &&
                    el.cornerRadius[1] == el.cornerRadius[2] &&
                    el.cornerRadius[2] == el.cornerRadius[3];

    if (allEqual) {
        j["corner_radius"] = el.cornerRadius[0];
    } else {
        j["corner_radius"] = json::array(
            {el.cornerRadius[0], el.cornerRadius[1], el.cornerRadius[2], el.cornerRadius[3]});
    }

    if (el.fill.Valid())
        j["fill"] = paintToJson(el.fill);
    if (el.stroke.Valid())
        j["stroke"] = paintToJson(el.stroke);

    if (el.inAnimation.type != AnimationType::None)
        j["anim_in"] = animDefToJson(el.inAnimation);
    if (el.outAnimation.type != AnimationType::None)
        j["anim_out"] = animDefToJson(el.outAnimation);

    if (el.mask)
        j["mask"] = el.mask->id;
    if (el.parent)
        j["parent"] = el.parent->id;

    if (el.fitToChildren) {
        j["fit_to_children"] = true;
        j["children_padding"] = json::array({el.childrenPadding[0], el.childrenPadding[1],
                                             el.childrenPadding[2], el.childrenPadding[3]});
    }

    // Text-specific fields (harmless to emit for rectangles too)
    if (el.type == ElementType::Text) {
        j["text"] = el.text;
        j["font_family"] = el.font.family;
        j["font_size"] = el.font.size;
        j["font_italic"] = el.font.isItalic;
        j["font_underline"] = el.font.isUnderline;
        j["font_strikethrough"] = el.font.isStrikethrough;
        j["font_weight"] = fontWeightToString(el.font.weight);
        j["auto_scale"] = el.autoScale;
        j["text_align_x"] = alignmentHToString(el.textAlignX);
        j["text_align_y"] = alignmentVToString(el.textAlignY);
        j["ellipsize"] = ellipsizeToString(el.ellipsize);
        j["wrap"] = wrapModeToString(el.wrapMode);
        j["text_transform"] = textTransformToString(el.transform);
    }

    return j;
}

static json graphicToJson(const Graphic& g)
{
    json j;
    j["id"] = g.id;
    j["z_order"] = g.zOrder;

    json elems = json::array();
    for (const auto& el : g.elements)
        elems.push_back(elementToJson(el));
    j["elements"] = elems;

    return j;
}

// ── SceneDocument ─────────────────────────────────────────────────────────────

SceneDocument::SceneDocument(QObject* parent) : QObject(parent) {}

bool SceneDocument::load(const QString& path)
{
    try {
        m_scene = Scene::Load(path.toStdString());
    } catch (const std::exception&) {
        return false;
    }

    // Parse name field separately (Scene struct has no name)
    m_sceneName.clear();
    try {
        std::ifstream f(path.toStdString());
        auto j = json::parse(f);
        if (j.contains("name") && j["name"].is_string())
            m_sceneName = j["name"].get<std::string>();
    } catch (...) {
    }

    // Build logical reference map from the raw JSON so we can survive
    // structural mutations.  We re-read the file a second time here to
    // extract mask/parent ids without the engine discarding them.
    m_elementRefs.clear();
    try {
        std::ifstream f2(path.toStdString());
        auto j = json::parse(f2);
        if (j.contains("graphics") && j["graphics"].is_array()) {
            for (const auto& gj : j["graphics"]) {
                std::string gid = gj.value("id", "");
                RefMap rm;
                if (gj.contains("elements") && gj["elements"].is_array()) {
                    for (const auto& ej : gj["elements"]) {
                        std::string eid = ej.value("id", "");
                        std::string maskId = ej.value("mask", "");
                        std::string parentId = ej.value("parent", "");
                        rm[eid] = {maskId, parentId};
                    }
                }
                m_elementRefs[gid] = std::move(rm);
            }
        }
    } catch (...) {
    }

    m_filePath = path;
    m_undoStack.clear();
    setModified(false);

    emit filePathChanged(m_filePath);
    emit documentChanged();
    return true;
}

bool SceneDocument::save()
{
    if (m_filePath.isEmpty())
        return false;
    return saveAs(m_filePath);
}

bool SceneDocument::saveAs(const QString& path)
{
    try {
        json j = sceneToJson(m_scene, m_sceneName);
        std::ofstream f(path.toStdString());
        if (!f.is_open())
            return false;
        f << j.dump(4);
    } catch (...) {
        return false;
    }

    if (path != m_filePath) {
        m_filePath = path;
        emit filePathChanged(m_filePath);
    }
    setModified(false);
    return true;
}

Element& SceneDocument::getElement(ElementRef ref)
{
    auto& el = scene().GetById(ref.graphicsId.toStdString()).GetById(ref.elementId.toStdString());
    return el;
}

QString SceneDocument::sceneName() const
{
    if (!m_sceneName.empty())
        return QString::fromStdString(m_sceneName);
    // Fall back to filename stem
    if (!m_filePath.isEmpty())
        return QFileInfo(m_filePath).baseName();
    return QString("Untitled");
}

void SceneDocument::setSceneName(const QString& name)
{
    m_sceneName = name.toStdString();
    setModified(true);
    emit documentChanged();
}

void SceneDocument::setElementRef(const std::string& graphicId, const std::string& elementId,
                                  const std::string& maskId, const std::string& parentId)
{
    m_elementRefs[graphicId][elementId] = {maskId, parentId};
    resolveElementPointers();
    setModified(true);
    emit documentChanged();
}

void SceneDocument::reset()
{
    m_scene = Scene::LoadString(R"({"name":"New Scene","graphics":[]})");
    m_sceneName = "New Scene";
    m_filePath.clear();
    m_elementRefs.clear();
    m_undoStack.clear();
    setModified(false);
    emit filePathChanged(m_filePath);
    emit documentChanged();
}

static void applyFitToChildrenPass(Scene& scene)
{
    for (auto& g : scene.graphics) {
        for (auto& el : g.elements) {
            if (!el.fitToChildren)
                continue;
            std::vector<Element*> children;
            for (auto& other : g.elements)
                if (other.parent == &el)
                    children.push_back(&other);
            if (children.empty())
                continue;

            double minX = children[0]->bounds.x, minY = children[0]->bounds.y;
            double maxX = minX + children[0]->bounds.width;
            double maxY = minY + children[0]->bounds.height;
            for (auto* c : children) {
                minX = std::min(minX, c->bounds.x);
                minY = std::min(minY, c->bounds.y);
                maxX = std::max(maxX, c->bounds.x + c->bounds.width);
                maxY = std::max(maxY, c->bounds.y + c->bounds.height);
            }

            const float pT = el.childrenPadding[0], pR = el.childrenPadding[1];
            const float pB = el.childrenPadding[2], pL = el.childrenPadding[3];

            const double dx = minX - pL;
            const double dy = minY - pT;

            el.bounds.x += dx;
            el.bounds.y += dy;
            el.bounds.width = maxX - minX + pL + pR;
            el.bounds.height = maxY - minY + pT + pB;

            for (auto* c : children) {
                c->bounds.x -= dx;
                c->bounds.y -= dy;
            }
        }
    }
}

void SceneDocument::applyMutation(std::function<void(Scene&)> fn)
{
    fn(m_scene);
    resolveElementPointers();
    applyFitToChildrenPass(m_scene);
    setModified(true);
    emit documentChanged();
}

// static
nlohmann::json SceneDocument::sceneToJson(const Scene& scene, const std::string& name)
{
    json j;
    if (!name.empty())
        j["name"] = name;

    j["width"] = scene.width;
    j["height"] = scene.height;

    json graphics = json::array();
    for (const auto& g : scene.graphics)
        graphics.push_back(graphicToJson(g));
    j["graphics"] = graphics;

    return j;
}

// private

void SceneDocument::resolveElementPointers()
{
    for (auto& g : m_scene.graphics) {
        auto gRefIt = m_elementRefs.find(g.id);
        if (gRefIt == m_elementRefs.end())
            continue;
        const RefMap& rm = gRefIt->second;
        for (auto& el : g.elements) {
            el.mask = nullptr;
            el.parent = nullptr;
            auto it = rm.find(el.id);
            if (it == rm.end())
                continue;
            const auto& [maskId, parentId] = it->second;
            if (!maskId.empty())
                el.mask = findElementById(g, maskId);
            if (!parentId.empty())
                el.parent = findElementById(g, parentId);
        }
    }
}

void SceneDocument::renameGraphicRef(const std::string& oldId, const std::string& newId)
{
    auto it = m_elementRefs.find(oldId);
    if (it != m_elementRefs.end()) {
        m_elementRefs[newId] = std::move(it->second);
        m_elementRefs.erase(it);
    }
}

void SceneDocument::setModified(bool modified)
{
    if (m_modified == modified)
        return;
    m_modified = modified;
    emit modifiedChanged(m_modified);
}

// static
Element* SceneDocument::findElementById(Graphic& g, const std::string& id)
{
    for (auto& el : g.elements)
        if (el.id == id)
            return &el;
    return nullptr;
}
