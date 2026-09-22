CSSX Performance 1.0.0
Live graphics settings for A/B testing, inside the CSSX tab.

Needs CSSX 1.0.0. Extract to ue4ss/Mods/CSSX/extensions/ so you get
extensions/cssx.performance/{extension.json, main.lua, menu.json}.

Every control is an engine console variable applied at once. The status line
shows the frame rate and CSSX's own cost. Restore puts the game's values back.
"Re-apply at launch" keeps your choices; otherwise the game's own options
return after a restart. Changing the game's options menu overrides these.
