#include "widget_container.hpp"
#include "desk_config.hpp"
#include <algorithm>

namespace miqudesk {

WidgetContainer::WidgetContainer(std::shared_ptr<Widget> widget, int x, int y, int width, int height)
    : m_widget(std::move(widget)), m_x(x), m_y(y), m_width(width), m_height(height) {}

bool WidgetContainer::contains(int canvas_x, int canvas_y) const {
    return canvas_x >= m_x && canvas_x < (m_x + m_width) &&
           canvas_y >= m_y && canvas_y < (m_y + m_height);
}

void WidgetContainer::tick() {
    if (m_widget) {
        m_widget->tick();
    }
}

void WidgetContainer::on_config_reload() {
    if (m_widget) {
        m_widget->on_config_reload();
    }
}

void WidgetContainer::draw(cairo_t* cr) {
    if (!cr || !m_widget) return;

    miqu::Rect bounds(m_x, m_y, m_width, m_height);
    auto view = m_widget->get_view();
    if (view) {
        view->draw(cr, bounds);
    }

    // Subtle resize grip in bottom-right corner (if resizable)
    if (m_widget && m_widget->is_resizable()) {
        cairo_save(cr);
        double grip_alpha = (m_is_resizing || m_hover_resize) ? 0.80 : 0.22;
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, grip_alpha);
        cairo_set_line_width(cr, 1.5);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

        int rx = m_x + m_width;
        int ry = m_y + m_height;
        // Outer diagonal grip tick
        cairo_move_to(cr, rx - 14, ry - 6);
        cairo_line_to(cr, rx - 6, ry - 14);
        // Inner diagonal grip tick
        cairo_move_to(cr, rx - 10, ry - 6);
        cairo_line_to(cr, rx - 6, ry - 10);
        cairo_stroke(cr);
        cairo_restore(cr);
    }

    // If dragging or resizing, draw an ambient accent highlight outline
    if (m_is_dragging || m_is_resizing) {
        cairo_save(cr);
        cairo_set_source_rgba(cr, 0.2f, 0.75f, 1.0f, 0.75f);
        cairo_set_line_width(cr, 2.0);
        double dashes[] = { 6.0, 4.0 };
        cairo_set_dash(cr, dashes, 2, 0.0);
        cairo_rectangle(cr, m_x - 1, m_y - 1, m_width + 2, m_height + 2);
        cairo_stroke(cr);
        cairo_restore(cr);
    }
}

bool WidgetContainer::handle_mouse_button(int canvas_x, int canvas_y, miqu::MouseButton button, bool pressed) {
    if (pressed) {
        if (!contains(canvas_x, canvas_y)) {
            return false;
        }

        // Only left button initiates drag or resize
        if (button != miqu::MouseButton::Left) {
            if (m_widget) {
                auto view = m_widget->get_view();
                if (view) {
                    miqu::Rect bounds(m_x, m_y, m_width, m_height);
                    return view->on_mouse_button(canvas_x, canvas_y, button, true, bounds);
                }
            }
            return false;
        }

        m_mouse_down = true;
        m_is_dragging = false;
        m_drag_start_x = canvas_x;
        m_drag_start_y = canvas_y;
        m_orig_x = m_x;
        m_orig_y = m_y;
        m_orig_w = m_width;
        m_orig_h = m_height;

        // Check for resize grip in bottom-right 24x24 area (if resizable)
        if (m_widget && m_widget->is_resizable() &&
            canvas_x >= (m_x + m_width - 24) && canvas_y >= (m_y + m_height - 24)) {
            m_is_resizing = true;
        } else {
            m_is_resizing = false;
        }

        return true;
    } else {
        // Button release
        if (m_is_resizing) {
            m_is_resizing = false;
            m_mouse_down = false;
            return true;
        }

        if (m_mouse_down) {
            m_mouse_down = false;
            if (m_is_dragging) {
                m_is_dragging = false;
                return true;
            }

            // Pure click without dragging -> dispatch to widget's view
            if (m_widget) {
                auto view = m_widget->get_view();
                if (view) {
                    miqu::Rect bounds(m_x, m_y, m_width, m_height);
                    view->on_mouse_button(canvas_x, canvas_y, button, true, bounds);
                    view->on_mouse_button(canvas_x, canvas_y, button, false, bounds);
                }
            }
            return true;
        }

        if (m_widget && contains(canvas_x, canvas_y)) {
            auto view = m_widget->get_view();
            if (view) {
                miqu::Rect bounds(m_x, m_y, m_width, m_height);
                return view->on_mouse_button(canvas_x, canvas_y, button, false, bounds);
            }
        }
    }
    return false;
}

bool WidgetContainer::handle_mouse_move(int canvas_x, int canvas_y) {
    int snap = DeskConfig::get().grid_snap;

    if (m_is_resizing) {
        int new_w = std::max(140, m_orig_w + (canvas_x - m_drag_start_x));
        int new_h = std::max(70, m_orig_h + (canvas_y - m_drag_start_y));
        if (snap > 1) {
            new_w = (new_w / snap) * snap;
            new_h = (new_h / snap) * snap;
        }
        m_width = new_w;
        m_height = new_h;
        return true;
    }

    if (m_mouse_down) {
        int dx = canvas_x - m_drag_start_x;
        int dy = canvas_y - m_drag_start_y;
        if (!m_is_dragging && (std::abs(dx) > 3 || std::abs(dy) > 3)) {
            m_is_dragging = true;
        }

        if (m_is_dragging) {
            int new_x = std::max(0, m_orig_x + dx);
            int new_y = std::max(0, m_orig_y + dy);
            if (snap > 1) {
                new_x = (new_x / snap) * snap;
                new_y = (new_y / snap) * snap;
            }
            m_x = new_x;
            m_y = new_y;
            return true;
        }
    }

    // Hover state over resize corner
    bool near_corner = m_widget && m_widget->is_resizable() &&
                       contains(canvas_x, canvas_y) &&
                       canvas_x >= (m_x + m_width - 24) &&
                       canvas_y >= (m_y + m_height - 24);
    if (near_corner != m_hover_resize) {
        m_hover_resize = near_corner;
        return true;
    }

    if (m_widget && contains(canvas_x, canvas_y)) {
        auto view = m_widget->get_view();
        if (view) {
            miqu::Rect bounds(m_x, m_y, m_width, m_height);
            return view->on_mouse_move(canvas_x, canvas_y, bounds);
        }
    }
    return false;
}

} // namespace miqudesk
