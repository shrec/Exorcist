#include "DockSideBar.h"
#include "DockSideTab.h"
#include "ExDockWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace exdock {

DockSideBar::DockSideBar(SideBarArea area, QWidget *parent)
    : QWidget(parent)
    , m_area(area)
{
    const bool vertical = (area == SideBarArea::Left || area == SideBarArea::Right);
    if (vertical) {
        m_layout = new QVBoxLayout(this);
        setFixedWidth(22);
    } else {
        m_layout = new QHBoxLayout(this);
        setFixedHeight(22);
    }
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(1);
    m_layout->addStretch();

    setObjectName(QStringLiteral("exdock-sidebar-%1")
                      .arg(static_cast<int>(area)));

    // Hidden until tabs are added
    hide();
}

DockSideTab *DockSideBar::addTab(ExDockWidget *dock)
{
    if (!dock) return nullptr;

    // Don't add duplicate tabs
    for (auto *t : m_tabs) {
        if (t->dockWidget() == dock)
            return t;
    }

    auto *tab = new DockSideTab(dock, m_area, this);

    // Insert before the stretch
    m_layout->insertWidget(m_layout->count() - 1, tab);
    m_tabs.append(tab);

    connect(tab, &QToolButton::clicked, this, [this, dock](bool checked) {
        // Uncheck other tabs in this bar
        if (checked) {
            for (auto *t : m_tabs) {
                if (t->dockWidget() != dock)
                    t->setChecked(false);
            }
        }
        emit tabClicked(dock, checked);
    });

    show();
    return tab;
}

void DockSideBar::removeTab(ExDockWidget *dock)
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i]->dockWidget() == dock) {
            auto *tab = m_tabs.takeAt(i);
            m_layout->removeWidget(tab);
            tab->deleteLater();
            break;
        }
    }
    if (m_tabs.isEmpty())
        hide();
}

DockSideTab *DockSideBar::tabFor(ExDockWidget *dock) const
{
    for (auto *tab : m_tabs) {
        if (tab->dockWidget() == dock)
            return tab;
    }
    return nullptr;
}

void DockSideBar::uncheckAll()
{
    for (auto *tab : m_tabs)
        tab->setChecked(false);
}

} // namespace exdock
