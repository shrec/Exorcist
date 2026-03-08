#pragma once

#include <QColor>
#include <QString>

// ── ChatThemeTokens ───────────────────────────────────────────────────────────
//
// Centralizes VS Code dark-modern color tokens used by the chat widgets.
// Maps --vscode-* CSS variables to named constants for use in Qt stylesheets
// and paint routines.
//
// Reference: ReserchRepos/copilot/src/extension/completions-core/
//            vscode-node/extension/src/panelShared/themes/dark-modern.ts

namespace ChatTheme
{
    // ── Panel / Editor ────────────────────────────────────────────────────
    inline constexpr auto PanelBg          = "#1f1f1f";
    inline constexpr auto EditorBg         = "#1f1f1f";
    inline constexpr auto InputBg          = "#313131";
    inline constexpr auto InputBorder      = "#3e3e42";
    inline constexpr auto InputFocusBorder = "#0078d4";
    inline constexpr auto SideBarBg        = "#181818";

    // ── Text ──────────────────────────────────────────────────────────────
    inline constexpr auto FgPrimary        = "#cccccc";
    inline constexpr auto FgSecondary      = "#9d9d9d";
    inline constexpr auto FgDimmed         = "#6a6a6a";
    inline constexpr auto FgMuted          = "#4a4a4a";
    inline constexpr auto FgBright         = "#e8e8e8";
    inline constexpr auto FgCode           = "#ce9178";

    // ── Links ─────────────────────────────────────────────────────────────
    inline constexpr auto LinkBlue         = "#4daafc";
    inline constexpr auto LinkVisited      = "#4daafc";

    // ── Accent ────────────────────────────────────────────────────────────
    inline constexpr auto AccentBlue       = "#0078d4";
    inline constexpr auto AccentBlueHover  = "#026ec1";
    inline constexpr auto AccentPurple     = "#5a4fcf";
    inline constexpr auto SelectionBg      = "#264f78";
    inline constexpr auto ListSelection    = "#094771";

    // ── Buttons ───────────────────────────────────────────────────────────
    inline constexpr auto ButtonBg         = "#0078d4";
    inline constexpr auto ButtonHover      = "#026ec1";
    inline constexpr auto ButtonFg         = "#ffffff";
    inline constexpr auto SecondaryBtnBg   = "#3a3d41";
    inline constexpr auto SecondaryBtnFg   = "#cccccc";
    inline constexpr auto SecondaryBtnHover= "#4a4d51";

    // ── Borders / Separators ──────────────────────────────────────────────
    inline constexpr auto Border           = "#3e3e42";
    inline constexpr auto SepLine          = "#2d2d2d";
    inline constexpr auto TabActiveBorder  = "#007acc";

    // ── Code blocks ───────────────────────────────────────────────────────
    inline constexpr auto CodeBg           = "#1a1a1a";
    inline constexpr auto CodeHeaderBg     = "#2d2d30";
    inline constexpr auto KeywordBlue      = "#569cd6";
    inline constexpr auto InlineCodeBg     = "#383838";

    // ── Status ────────────────────────────────────────────────────────────
    inline constexpr auto ErrorFg          = "#f14c4c";
    inline constexpr auto ErrorBg          = "rgba(241,76,76,0.06)";
    inline constexpr auto WarningFg        = "#cca700";
    inline constexpr auto SuccessFg        = "#4ec9b0";
    inline constexpr auto ProgressBlue     = "#007acc";

    // ── Thinking ──────────────────────────────────────────────────────────
    inline constexpr auto ThinkingBg       = "#1a1a2e";
    inline constexpr auto ThinkingBorder   = "#5a4fcf";
    inline constexpr auto ThinkingFg       = "#9d9dbd";
    inline constexpr auto ThinkingSummary  = "#7a7a9e";

    // ── Tool invocation ───────────────────────────────────────────────────
    inline constexpr auto ToolBg           = "#1e1e1e";
    inline constexpr auto ToolBorderDefault= "#3e3e42";
    inline constexpr auto ToolBorderActive = "#007acc";
    inline constexpr auto ToolBorderOk     = "#4ec9b0";
    inline constexpr auto ToolBorderFail   = "#f14c4c";

    // ── Diff ──────────────────────────────────────────────────────────────
    inline constexpr auto DiffInsertBg     = "rgba(40,120,40,0.2)";
    inline constexpr auto DiffDeleteBg     = "rgba(220,60,60,0.15)";
    inline constexpr auto DiffInsertFg     = "#4ec9b0";
    inline constexpr auto DiffDeleteFg     = "#f14c4c";
    inline constexpr auto DiffAdded        = "#4ec9b0";
    inline constexpr auto DiffRemoved      = "#f14c4c";
    inline constexpr auto DiffModified     = "#e2c08d";

    // ── Aliases (shorthand used by widgets) ───────────────────────────────
    inline constexpr auto LinkFg           = "#4daafc";
    inline constexpr auto AccentFg         = "#0078d4";
    inline constexpr auto WarningBg        = "rgba(204,167,0,0.08)";
    inline constexpr auto ScrollTrack      = "#2b2b2b";
    inline constexpr auto ScrollThumb      = "#424242";

    // ── Avatar ────────────────────────────────────────────────────────────
    inline constexpr auto AvatarUserBg     = "#0078d4";
    inline constexpr auto AvatarAiBg       = "#5a4fcf";
    // ── Slash command pills ──────────────────────────────────────
    inline constexpr auto SlashPillBg      = "#34414b";
    inline constexpr auto SlashPillFg      = "#40a6ff";
    // ── Premium / Billing ────────────────────────────────────────────────
    inline constexpr auto PremiumBadgeBg    = "#7c3aed";
    inline constexpr auto PremiumBadgeFg    = "#e0d4fc";
    inline constexpr auto PremiumFg         = "#a78bfa";
    inline constexpr auto MultiplierFg      = "#9d9d9d";

    // ── Scrollbar ─────────────────────────────────────────────────────────
    inline constexpr auto ScrollHandle      = "#424242";
    inline constexpr auto ScrollHandleHover = "#4f4f4f";
    inline constexpr auto ScrollHandlePressed = "#5a5a5a";

    // ── Disabled / Focus ──────────────────────────────────────────────────
    inline constexpr auto DisabledBg        = "#2a2a2a";
    inline constexpr auto DisabledFg        = "#4a4a4a";
    inline constexpr auto FocusOutline      = "#0078d4";
    inline constexpr auto ButtonPressed     = "#005a9e";
    inline constexpr auto HoverBg           = "#2a2d2e";

    // ── Fonts ─────────────────────────────────────────────────────────────
    inline const auto FontFamily     = QStringLiteral("'Segoe WPC','Segoe UI',Arial,sans-serif");
    inline const auto MonoFamily     = QStringLiteral("'Cascadia Code','Fira Code',Consolas,monospace");
    inline constexpr int FontSize    = 13;
    inline constexpr int CodeFontSize= 14;
    inline constexpr int SmallFont   = 12;
    inline constexpr int TinyFont    = 11;

    // ── Spacing ───────────────────────────────────────────────────────────
    inline constexpr int RadiusSmall = 2;
    inline constexpr int RadiusMedium= 4;
    inline constexpr int RadiusLarge = 8;
    inline constexpr int Padding     = 10;
    inline constexpr int AvatarSize  = 24;
    inline constexpr int AvatarRadius= 12;

} // namespace ChatTheme
