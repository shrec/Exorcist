# Copilot Chat — ვიზუალური პორტირების როადმეპი

> **სტატუსი**: ფუნქციონალურად 16/16 ფაზა დასრულებულია. ვიზუალური UI/UX არ ემთხვევა VS Code-ს.
>
> **მიზანი**: Exorcist-ის Chat UI ვიზუალურად იდენტური გახადოთ VS Code-ის Copilot Chat-თან.
>
> **წყარო**: VS Code workbench chat UI (CSS/layout reference) + `ReserchRepos/copilot/`

---

## 🔴 რატომ არ მუშაობს ეხლანდელი მიდგომა

წინა ცვლილებები სტრუქტურული იყო (provider tab-ების მოშლა, header bar-ის დამატება) მაგრამ
**ვიზუალურად ეკრანზე არაფერი შეცვლილა** რადგან:
- შეტყობინებების layout იგივე დარჩა
- ავატარების ზომა/ფერი იგივეა (emoji 👤/✨ vs VS Code-ის ტექსტ-ინიციალ ავატარი)
- Input field-ის ჩარჩო არ იცვლება focus-ზე
- კოდის ბლოკებს არ აქვთ VS Code-ის ცისფერი border-left
- Separator ხაზი user/assistant-ს შორის VS Code-ში საერთოდ არ არის
- Follow-up ბმულები pill-ებად ჩანს, VS Code-ში ტექსტ-ბმულებია
- Streaming-ის progress bar არ არსებობს
- Padding/margin ძალიან დიდია — ვიჯეტი "სქელი" გამოჩნდება

---

## მომზადებული ფაზები

### ფაზა V1 — Message Layout (ყველაზე თვალსაჩინო ცვლილება) 🔴

**ფაილი**: `src/agent/chat/chatturnwidget.h`

| # | ცვლილება | VS Code | Exorcist (ეხლა) | პრიორიტეტი |
|---|---------|---------|-----------------|-----------|
| 1.1 | **ავატარი ზომა** | 16×16px, border-radius:4px | 24×24px, border-radius:12px (circle) | 🔴 |
| 1.2 | **ავატარი ტიპი** | ტექსტ-ინიციალი (C/U) ან SVG ლოგო | Emoji 👤/✨ | 🔴 |
| 1.3 | **Separator user↔assistant** | არ არის, მხოლოდ 8px ვერტიკალური სივრცე | 1px solid #3e3e42 ხაზი | 🔴 |
| 1.4 | **Root margins** | `padding: 2px 20px 2px 6px` (ძალიან ტაიტი) | `margins: 12px 8px 12px 8px` (სქელი) | 🔴 |
| 1.5 | **Content left margin** | 22px (avatar_width + gap) | 26px | 🟡 |
| 1.6 | **User/Assistant header gap** | 6px | spacing:6px (ok) | ✅ |
| 1.7 | **Font sizes** | 13px body, 12px labels, 11px detail | 13px/12px/11px (ok) | ✅ |
| 1.8 | **Feedback row პოზიცია** | absolute top-right, on hover | bottom of content, on hover | 🟡 |

**კონკრეტული ტექნიკური ცვლილებები**:
```cpp
// 1.1 + 1.2: ავატარი → ტექსტი სკვერ ბოქსში
auto *userAvatar = new QLabel("U", this);
userAvatar->setFixedSize(16, 16);
userAvatar->setAlignment(Qt::AlignCenter);
userAvatar->setStyleSheet("font-size:10px; font-weight:700; color:white;"
    "background:#0078d4; border-radius:4px;");

auto *assistAvatar = new QLabel("✦", this);
assistAvatar->setFixedSize(16, 16);
assistAvatar->setAlignment(Qt::AlignCenter);
assistAvatar->setStyleSheet("font-size:10px; font-weight:700; color:white;"
    "background:#5a4fcf; border-radius:4px;");

// 1.3: Separator — წაშლა
// sep widget-ის ნაცვლად 8px spacing

// 1.4: Root margins — ტაიტი
root->setContentsMargins(6, 4, 20, 4); // VS Code-ის სტილი
```

---

### ფაზა V2 — Input Field Focus Border 🔴

**ფაილი**: `src/agent/chat/chatinputwidget.cpp`

| # | ცვლილება | VS Code | Exorcist (ეხლა) |
|---|---------|---------|-----------------|
| 2.1 | **Input block border** | 1px solid #3c3c3c (normal) → 1px solid #0078d4 (focus) | 1px solid #3e3e42 (static) |
| 2.2 | **Input block bg** | #1e1e1e (Monaco editor bg) | #2b2b2b (ScrollTrack) |
| 2.3 | **Input block border-radius** | 2px | 8px |
| 2.4 | **Attach button position** | Bottom bar left (📎 icon) | Input row left (⊕ text) |
| 2.5 | **Send button style** | Rounded, larger icon (→ arrow) | Square(ish), text ↑ |

**კონკრეტული ტექნიკური ცვლილებები**:
```cpp
// 2.1: inputBlock — dynamic focus border via QSS
// Need event filter on m_input for focusIn/focusOut to change inputBlock border
// OR use QSS subcontrol: "QWidget#inputBlock:focus-within" (Qt doesn't support this)
// Solution: event filter that toggles inputBlock stylesheet

// 2.2 + 2.3: Visual tweak
inputBlock->setStyleSheet("QWidget#inputBlock {"
    "background:#1e1e1e; border:1px solid #3c3c3c;"
    "border-radius:4px; margin:4px 8px 8px 8px; }");

// On focus:
inputBlock->setStyleSheet("...border:1px solid #0078d4;...");
```

---

### ფაზა V3 — Code Blocks Left Border 🔴

**ფაილი**: `src/agent/chat/chatmarkdownwidget.h` (defaultCss method)

| # | ცვლილება | VS Code | Exorcist (ეხლა) |
|---|---------|---------|-----------------|
| 3.1 | **pre border-left** | 3px solid #0e639c (dark blue) | არ არის |
| 3.2 | **pre border-radius** | 2px (uniform) | 0 0 4px 4px (bottom only) |
| 3.3 | **code-header border-radius** | 2px 2px 0 0 (top only) | 4px 4px 0 0 |
| 3.4 | **Inline code font-size** | 13px (same as body) | 12px (too small) |

**კონკრეტული ტექნიკური ცვლილებები**:
```cpp
// defaultCss() — pre element:
"pre { background:%4; padding:10px 12px;"
"  border-radius:0 0 2px 2px;"           // Was: 0 0 4px 4px
"  border-left:3px solid #0e639c;"       // NEW: blue left accent
"  font-family:%5; font-size:14px; ... }"

// code-header:  
".code-header { ... border-radius:2px 2px 0 0; ... }" // Was: 4px 4px 0 0

// Inline code:
"code { ... font-size:13px; ... }"  // Was: 12px
```

---

### ფაზა V4 — Followup Suggestions → Text Links 🟡

**ფაილი**: `src/agent/chat/chatfollowupswidget.h`

| # | ცვლილება | VS Code | Exorcist (ეხლა) |
|---|---------|---------|-----------------|
| 4.1 | **Layout** | VBoxLayout (ვერტიკალურად) | HBoxLayout (ჰორიზონტალურად) |
| 4.2 | **Styling** | Transparent bg, blue text (#4daafc), underline on hover | Pill: bg=#3a3d41, border, border-radius:12px |
| 4.3 | **Font** | 12px, no padding | 12px, padding:4px 12px |

**კონკრეტული ტექნიკური ცვლილებები**:
```cpp
// Layout → QVBoxLayout instead of QHBoxLayout
m_layout = new QVBoxLayout(this);
m_layout->setContentsMargins(0, 6, 0, 2);
m_layout->setSpacing(2);

// Button style → text link, NOT pill
"QToolButton { background:transparent; border:none; color:#4daafc;"
"  font-size:12px; padding:2px 0; text-align:left; }"
"QToolButton:hover { text-decoration:underline; color:white; }"
```

---

### ფაზა V5 — Streaming Progress Bar 🟡

**ფაილი**: `src/agent/chat/chatpanelwidget.cpp` + `chatpanelwidget.h`

| # | ცვლილება | VS Code | Exorcist (ეხლა) |
|---|---------|---------|-----------------|
| 5.1 | **Progress bar** | 2px ჰორიზონტალური ანიმაცია input-ის ქვეშ | არ არის |
| 5.2 | **Color** | #007acc (blue) animating 10%→80% width | N/A |

**კონკრეტული ტექნიკური ცვლილებები**:
```cpp
// New member: QProgressBar or QWidget with animation
m_streamingProgress = new QWidget(this);
m_streamingProgress->setFixedHeight(2);
m_streamingProgress->setStyleSheet("background:#007acc;");
m_streamingProgress->hide();
// In rootLayout: add above m_inputWidget

// Animation: QPropertyAnimation on width or QSS gradient shift
// Show on streaming start, hide on finish/cancel
```

---

### ფაზა V6 — Welcome Screen Polish 🟡

**ფაილი**: `src/agent/chat/chatwelcomewidget.h`

| # | ცვლილება | VS Code | Exorcist (ეხლა) |
|---|---------|---------|-----------------|
| 6.1 | **Title** | "Ask Copilot" | "Copilot" |
| 6.2 | **Subtitle** | "Pick a suggestion below or start typing" | "Ask questions, generate code, or..." |
| 6.3 | **Suggestion pills** | Rounded pills with / prefix, `border-radius:14px` | Rectangular, `border-radius:6px` |
| 6.4 | **Suggestion content** | `/explain`, `/fix`, `/tests`, `/review`, `/doc` | Natural language sentences |
| 6.5 | **Layout width** | max-width:300px per pill | fills parent width |

**კონკრეტული ტექნიკური ცვლილებები**:
```cpp
// Title → "Ask Copilot"
title->setText(tr("Ask Copilot"));

// Subtitle → concise
subtitle->setText(tr("Pick a suggestion or just start typing"));

// Pills → slash command format, compact
suggestions = { "/explain — Explain this code",
                "/fix — Fix bugs",
                "/tests — Generate tests",
                "/review — Review code",
                "/doc — Add documentation" };

// Pill styling → rounded pill, smaller
"border-radius:14px; padding:6px 14px; max-width:300px;"
"font-size:12px; text-align:left;"
```

---

### ფაზა V7 — Input Focus Border (Event Filter) 🔴

**ფაილი**: `src/agent/chat/chatinputwidget.cpp`

ეს ფაზა V2-ის რეალიზაციაა — input-ის focus/blur ის დროს border-ის ფერის ცვლა.

```cpp
// eventFilter() — handle FocusIn/FocusOut for m_input
if (obj == m_input) {
    if (ev->type() == QEvent::FocusIn) {
        m_inputBlock->setStyleSheet("...border:1px solid #0078d4;...");
    } else if (ev->type() == QEvent::FocusOut) {
        m_inputBlock->setStyleSheet("...border:1px solid #3c3c3c;...");
    }
}
```

---

### ფაზა V8 — Thinking Widget Polish 🟢

**ფაილი**: `src/agent/chat/chatthinkingwidget.h`

| # | ცვლილება | VS Code | Exorcist (ეხლა) |
|---|---------|---------|-----------------|
| 8.1 | **Header icon** | 💡 → ტექსტ "Thinking" | 💡 Thinking… (emoji) |
| 8.2 | **Max height** | 200px, scrollable | No max, grows forever |
| 8.3 | **Auto-collapse** | Already works ✅ | ✅ |

---

### ფაზა V9 — Overall Spacing Tighten 🟡

გლობალური spacing/padding-ის დაპატარავება VS Code-ის ტაიტი layout-თან შესადარებელი.

| Widget | Property | VS Code | Exorcist |
|--------|----------|---------|---------|
| ChatTurnWidget root | margins | 6,4,20,4 | 12,8,12,8 |
| User message | padding-left | 22px | 26px |
| Assistant content | padding-left | 22px | 26px |
| Feedback row | margins | 22,2,0,2 | 26,4,0,2 |
| Code block `pre` | padding | 10px 12px | 10px 12px ✅ |
| Separator user↔asst | height | 0 (no sep) | 1px |

---

## 📋 პრიორიტეტული რიგი (Top → Bottom)

| # | ფაზა | ვიზუალური ეფექტი | სირთულე |
|---|------|-----------------|---------|
| **1** | V1 Message Layout | ⭐⭐⭐⭐⭐ — ყველაზე თვალსაჩინო | საშუალო |
| **2** | V3 Code Block Border | ⭐⭐⭐⭐ — კოდი ძალიან ბევრია | მარტივი |
| **3** | V2/V7 Input Focus Border | ⭐⭐⭐⭐ — ყოველ ინტერაქციაზე ჩანს | საშუალო |
| **4** | V4 Followup → Text Links | ⭐⭐⭐ — ბოლო response-ზე ჩანს | მარტივი |
| **5** | V5 Streaming Progress | ⭐⭐⭐ — streaming-ის დროს | საშუალო |
| **6** | V6 Welcome Screen | ⭐⭐ — მხოლოდ ცარიელ state-ზე | მარტივი |
| **7** | V9 Overall Spacing | ⭐⭐⭐ — ყველაფერზე მოქმედებს | მარტივი |
| **8** | V8 Thinking Polish | ⭐ — იშვიათად ჩანს | მარტივი |

---

## შეფასება

- **V1 + V3 + V9** = ყველაზე დიდი ვიზუალური impact — ეს სამი ფაზა ერთად გააკეთებს ნამდვილ ვიზუალურ ტრანსფორმაციას
- **V2/V7** = ინტერაქტიული polish — Focus border როცა კლავიატურაზე წერ
- **V4 + V5 + V6** = secondary polish
- **V8** = nice to have

**რეკომენდაცია**: V1 → V3 → V9 → V7 → V4 → V5 → V6 → V8 (ამ თანმიმდევრობით)
