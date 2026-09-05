#pragma once

#include <QString>
#include <QStringList>

// ---------------------------------------------------------------------------
// External launchers shared by MainWindow's task links and the detail pages'
// sidebar links. Task and sidebar links with no dedicated page in this app hand
// off to Linux equivalents of Windows Control Panel settings.
// ---------------------------------------------------------------------------

// Desktop Background
inline QStringList desktopBackground()
{
    return { "deskprop" };
}

// Window Color - use system color dialog
inline QStringList windowColor()
{
    return { "nwg-look" };
}

// Sounds - use pavucontrol
inline QStringList sounds()
{
    return { "pavucontrol" };
}

// Screen Saver - use system screensaver
inline QStringList screenSaver()
{
    return { "deskprop" };
}

// Mouse Settings - use system mouse settings (gnome-mouse-properties or xinput)
inline QStringList mouseSettings()
{
    return { "gnome-mouse-properties" };
}

// Accessibility - use system accessibility settings
inline QStringList accessibility()
{
    return { "gnome-control-center", "universal-access" };
}

// User Accounts - use system user management
inline QStringList userAccounts()
{
    return { "system-config-users" };
}

// Display - use wdisplays or xrandr
inline QStringList displaySettings()
{
    return { "wdisplays" };
}

// Time and Date - use system clock settings
inline QStringList timeDate()
{
    return { "gnome-datetime-control" };
}

// Network Settings - use system network settings
inline QStringList networkSettings()
{
    return { "nm-connection-editor" };
}

// Firewall - use system firewall settings
inline QStringList firewallSettings()
{
    return { "gufw" };
}

// Device Manager - use system device manager
inline const QStringList deviceManagerCmd = {
    QStringLiteral("devmgmt")
};

// Desktop Gadgets links - KDE widgets
inline const QStringList kWidgetExplorerCmd = {
    QStringLiteral("qdbus6"), QStringLiteral("org.kde.plasmashell"),
    QStringLiteral("/PlasmaShell"),
    QStringLiteral("org.kde.PlasmaShell.toggleWidgetExplorer")
};
inline const QStringList kGetWidgetsCmd = {
    QStringLiteral("knewstuff-dialog6"),
    QStringLiteral("/usr/share/knsrcfiles/plasmoids.knsrc")
};

// Font Settings - use system font settings
inline QStringList fontSettings()
{
    return { "gnome-tweaks", "--page=fonts" };
}

// Keyboard Settings - use system keyboard settings
inline QStringList keyboardSettings()
{
    return { "gnome-control-center", "keyboard" };
}

// Region and Language - use system language settings
inline QStringList regionAndLanguage()
{
    return { "gnome-control-center", "region&language" };
}

// Credential Manager - use system credential manager
inline QStringList credentialManager()
{
    return { "gnome-keyring-manager" };
}

// Color Themes - use system theme settings
inline QStringList colorThemes()
{
    return { "gnome-tweaks", "--page=extensions" };
}

// Cursor Theme - use system cursor settings
inline QStringList cursorTheme()
{
    return { "gnome-tweaks", "--page=mouse" };
}

// High Contrast - use system accessibility settings
inline QStringList highContrast()
{
    return { "gnome-control-center", "accessibility" };
}

// Device Manager - use system device manager
inline QStringList deviceManager()
{
    return { "devmgmt" };
}
