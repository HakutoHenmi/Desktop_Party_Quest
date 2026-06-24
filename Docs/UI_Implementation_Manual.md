# UI Implementation Manual & Recipe Book

<div style="background-color: #FFFFFF; color: #333333; padding: 20px; font-family: sans-serif;">

## 1. Overview
This manual covers the implementation of common UI components in Desktop Party Quest, such as HP gauges and Damage Popups. The system is designed to be lightweight, componentized, and easy to maintain.

**Note:** All documentation must adhere to a strict light theme (white/light gray background) as per team rules.

## 2. Common Components

### 2.1 HP Gauge (`UI_HPGauge`)
The HP Gauge is used for player characters (Jobs) and enemies.
- **Rendering:** Uses ImGui's draw list or custom shader.
- **Data Binding:** Binds to the Entity's `HealthComponent`.
- **Styling:** Colors dynamically change based on health percentage (e.g., Green > 50%, Yellow > 20%, Red < 20%).

### 2.2 Damage Popups (`UI_DamagePopup`)
Displays damage numbers above the entity.
- **Animation:** Pops up and fades out over 1.0 second.
- **Font:** Uses dynamic glyph caching for crisp rendering at different scales.
- **Instancing:** Uses object pooling or transient ECS entities to handle many popups efficiently.

## 3. Implementation Guidelines

### 3.1 Transparent Background Rendering
Since the game runs directly on the desktop with a transparent background (`WS_EX_LAYERED`, `DXGI_ALPHA_MODE_PREMULTIPLIED`), all UI elements must handle alpha correctly.
- Ensure text outlines are pre-multiplied correctly.
- Do not use dark opaque backgrounds for tooltips unless explicitly designed with a semi-transparent alpha.

### 3.2 ImGui Best Practices
- Use `ImGui::SetNextWindowBgAlpha(0.0f)` to make ImGui windows transparent.
- Rely on `ImDrawList` for custom shapes and UI elements tied to game entities.

## 4. Work Energy Redesign (UI Aspects)
The "Focus Aura" is triggered after 3+ seconds of input stop. This aura should be rendered behind the character portrait/icon.

## 5. UGC Integration
Face icons are loaded from `/Assets/Faces/`. The `CircleClipPS` shader is used to clip these icons into perfect circles. Ensure UI images bound to these textures are rendered using our custom shader pipeline or correctly mapped to ImGui image calls.

</div>
