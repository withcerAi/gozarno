// SPDX-License-Identifier: GPL-2.0-or-later
#include <QApplication>
#include <QPalette>
#include <QColor>
#include "ClientTheme.h"
#include "ClientLanguage.h"
void applyGozarnoTheme(bool dark)
{
    const QString background=dark ? "#0d1519" : "#edf3f1";
    const QString surface=dark ? "#152126" : "#ffffff";
    const QString foreground=dark ? "#eef5f2" : "#172c27";
    const QString muted=dark ? "#9aada9" : "#61756f";
    const QString border=dark ? "#2a393e" : "#dce6e2";
    const QString field=dark ? "#101b20" : "#f6f9f8";
    QPalette palette;
    palette.setColor(QPalette::Window,QColor(background)); palette.setColor(QPalette::WindowText,QColor(foreground));
    palette.setColor(QPalette::Base,QColor(field)); palette.setColor(QPalette::AlternateBase,QColor(surface));
    palette.setColor(QPalette::Text,QColor(foreground)); palette.setColor(QPalette::Button,QColor(surface)); palette.setColor(QPalette::ButtonText,QColor(foreground));
    palette.setColor(QPalette::Highlight,QColor("#216452")); palette.setColor(QPalette::HighlightedText,Qt::white);
    palette.setColor(QPalette::PlaceholderText,QColor(muted)); palette.setColor(QPalette::Disabled,QPalette::Text,QColor(muted));
    qApp->setPalette(palette);
    qApp->setStyleSheet(QStringLiteral(R"CSS(
        QMainWindow, QDialog { background: %1; color: %3; }
        QWidget { color: %3; font-family: 'Segoe UI'; font-size: 13px; }
        QWidget#body, QWidget#centralWidget { background: %1; }
        QFrame[card="true"], QWidget[card="true"] { background: %2; border: 1px solid %5; border-radius: 18px; }
        QLabel { background: transparent; border: none; }
        QLabel[role="pageTitle"] { font-size: 28px; font-weight: 600; }
        QLabel[role="hero"] { font-size: 25px; font-weight: 600; color: #eff8f4; }
        QLabel[role="section"] { font-size: 17px; font-weight: 600; }
        QLabel[role="muted"] { color: %4; font-size: 13px; }
        QLabel[role="eyebrow"] { color: %4; font-size: 10px; font-weight: 600; letter-spacing: 1px; }
        QLabel[role="value"] { font-size: 14px; font-weight: 500; }
        QLabel[role="metric"] { font-size: 22px; font-weight: 600; }
        QLabel[role="footer"] { color: %4; font-size: 11px; }
        QFrame#sidebar { background: #0c1917; border: none; }
        QLabel[role="wordmark"] { color: #f0f8f4; font-size: 29px; font-weight: 700; padding-left: 12px; }
        QLabel[role="railMuted"] { color: #829d94; font-size: 11px; padding-left: 12px; }
        QLabel[role="railStatus"] { color: #99b4a9; font-size: 12px; padding: 12px; }
        QPushButton#navButton { background: transparent; border: none; color: #a2b7af; text-align: left; padding: 12px 14px; border-radius: 10px; }
        QPushButton#navButton:hover { background: #152c25; color: #f5fff9; }
        QPushButton#navButton:checked { background: #1b3c31; color: #78e4bf; font-weight: 600; }
        QTabWidget#pageStack::pane { border: none; background: transparent; }
        QTabWidget::pane { border: 1px solid %5; border-radius: 12px; background: %2; }
        QTabBar::tab { background: transparent; color: %4; padding: 12px 20px; border-bottom: 2px solid transparent; }
        QTabBar::tab:selected { color: %3; border-bottom: 2px solid #64deb4; }
        QScrollArea#pageScroll, QScrollArea#pageScroll > QWidget > QWidget { background: transparent; border: none; }
        QFrame#connectionCard { background: #102b24; border: 1px solid #25483d; border-radius: 18px; }
        QFrame#connectionCard QLabel[role="muted"], QFrame#connectionCard QLabel[role="eyebrow"] { color: #a0bdb1; }
        QLabel[role="badge"] { color: #bdd6ca; font-size: 10px; font-weight: 600; letter-spacing: 1px; }
        QLabel[role="badge"][online="true"] { color: #70edc3; }
        QFrame#divider { background: %5; border: none; }
        QLineEdit, QComboBox, QSpinBox { background: %6; border: 2px solid %5; border-radius: 9px; padding: 10px 12px; min-height: 20px; selection-background-color: #286d59; }
        QComboBox::drop-down { border: none; width: 26px; }
        QComboBox QAbstractItemView, QMenu { background: %2; border: 1px solid %5; padding: 6px; selection-background-color: #286d59; color: %3; }
        QPushButton, QToolButton { background: %2; border: 2px solid %5; border-radius: 9px; padding: 10px 14px; font-weight: 500; }
        QPushButton:hover, QToolButton:hover { background: %6; border-color: #5e9d89; }
        QPushButton:pressed { background: #245b48; color: #ffffff; }
        QPushButton:checked { background: #254f40; color: #7ce4bd; border-color: #438c70; }
        QPushButton[quiet="true"] { background: transparent; }
        QPushButton[primary="true"], QPushButton#connectionButton { background: #64deb4; color: #08271c; border: 1px solid #64deb4; font-weight: 600; }
        QPushButton[primary="true"]:hover, QPushButton#connectionButton:hover { background: #85eac6; border-color: #85eac6; }
        QPushButton:disabled, QToolButton:disabled { color: #7c9188; border-color: %5; background: %6; }
        QPushButton#connectionButton:disabled { background: #244b3c; color: #8aab9c; border-color: #315a48; }
        QTableWidget { background: %2; border: 1px solid %5; border-radius: 12px; gridline-color: %5; selection-background-color: #244e40; selection-color: #f3fff9; }
        QHeaderView::section { background: %6; color: %4; border: none; border-bottom: 1px solid %5; padding: 14px 10px; font-weight: 500; }
        QTableWidget::item { padding: 10px; border: none; }
        QCheckBox { spacing: 12px; padding: 8px 0; min-height: 22px; }
        QCheckBox::indicator, QGroupBox::indicator { width: 20px; height: 20px; }
        QCheckBox::indicator:unchecked, QGroupBox::indicator:unchecked { image: url(:/images/gozarno/check-off.png); }
        QCheckBox::indicator:checked, QGroupBox::indicator:checked { image: url(:/images/gozarno/check-on.png); }
        QCheckBox::indicator:disabled:unchecked, QGroupBox::indicator:disabled:unchecked { image: url(:/images/gozarno/check-disabled-off.png); }
        QCheckBox::indicator:disabled:checked, QGroupBox::indicator:disabled:checked { image: url(:/images/gozarno/check-disabled-on.png); }
        QScrollBar:vertical { background: transparent; width: 7px; margin: 2px; }
        QScrollBar::handle:vertical { background: #4b685b; border-radius: 3px; min-height: 30px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
        QLineEdit:focus, QComboBox:focus, QPushButton:focus, QToolButton:focus, QSpinBox:focus { border: 2px solid #68c6a6; }
        QToolTip { background: %2; color: %3; border: 1px solid %5; padding: 8px; }
    )CSS").arg(background,surface,foreground,muted,border,field)
        .replace("text-align: left",ClientLanguage::isPersian() ? "text-align: right" : "text-align: left")
        .replace("padding-left: 12px",ClientLanguage::isPersian() ? "padding-right: 12px" : "padding-left: 12px")
        .replace("letter-spacing: 1px",ClientLanguage::isPersian() ? "letter-spacing: 0px" : "letter-spacing: 1px"));
}
