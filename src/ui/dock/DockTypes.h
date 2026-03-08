#pragma once

/// MIT License – Exorcist IDE Docking System
/// Shared types and enums for the docking framework.

#include <Qt>

namespace exdock {

/// Where a dock widget can be dropped relative to a dock area.
enum class DropArea {
    None   = 0x00,
    Left   = 0x01,
    Right  = 0x02,
    Top    = 0x04,
    Bottom = 0x08,
    Center = 0x10,   // tabify
    Outer  = 0x20    // app-level edge
};

/// Sidebar areas for auto-hide.
enum class SideBarArea {
    Left,
    Right,
    Top,
    Bottom,
    None
};

/// Dock widget state.
enum class DockState {
    Docked,
    Floating,
    AutoHidden,
    Closed
};

/// Toolbar docking edge.
enum class ToolBarEdge {
    Top,
    Bottom,
    Left,
    Right
};

/// Convert a Qt::DockWidgetArea to SideBarArea.
inline SideBarArea toSideBarArea(Qt::DockWidgetArea area) {
    switch (area) {
    case Qt::LeftDockWidgetArea:   return SideBarArea::Left;
    case Qt::RightDockWidgetArea:  return SideBarArea::Right;
    case Qt::TopDockWidgetArea:    return SideBarArea::Top;
    case Qt::BottomDockWidgetArea: return SideBarArea::Bottom;
    default:                       return SideBarArea::None;
    }
}

} // namespace exdock
