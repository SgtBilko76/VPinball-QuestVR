# VR room overrides (Meta Quest)

Some VR tables ship with their VR room switched off in the table script (`Const VRRoom = 0`), which hides their cabinet, backbox and backglass in VR. These files are copies of the table scripts with the VR room switched on, usually to the lightest **Ultra Minimal** room (cabinet, backbox and backglass without room walls).

VPX automatically loads a `.vbs` file named like the table and placed next to it, instead of the script built into the `.vpx`. The table file itself is not modified: delete the `.vbs` to get the original script back.

## Install

Copy the `.vbs` next to the matching `.vpx` on the Quest, for example with adb:

```
adb push "quest-overrides/Indiana Jones/Indiana Jones - The Pinball Adventure (Williams 1993) VPW 1.1 MOD VR.vbs" "/sdcard/VPinball/tables/Indiana Jones/"
```

The file name must match the `.vpx` file name, and the override only fits **this exact table version** (the whole script is replaced): with another version of a table, apply the same change to its own script instead.

## Tables

| Table | File | Changes |
|---|---|---|
| Austin Powers | `Austin Powers (Stern 2001) VPW 1.0.3 MOD VR.vbs` | line 72: `Const VRRoom = 1`<br>line 117: `If VRRoom = 1 Then 'Turns on VR Room and Cabinet` |
| Black Rose | `Black Rose (Bally 1992) VPW 1.4 MOD VR.vbs` | line 60: `Const VRRoom = 3` |
| Doctor Who | `Doctor Who (Bally 1992) VPW 1.1 MOD VR.vbs` | line 23: `Const VRRoom = 2`<br>line 4805: `PinCab_Backglass.visible = 1` |
| Indiana Jones | `Indiana Jones - The Pinball Adventure (Williams 1993) VPW 1.1 MOD VR.vbs` | line 28: `Const VRRoom = 2` |
| Judge Dredd | `Judge Dredd (Bally 1993) VPW 1.1 VR.vbs` | line 73: `Const VRRoom = 3`<br>line 4898: `PinCab_Backglass.visible = 1` |
| Metallica | `Metallica (Premium Monsters) (Stern 2013) Byancey 1.2 MOD.vbs` | line 102: `Const VRRoom = 3` |
| Simpsons Pinball Party | `Simpsons Pinball Party, The (Stern 2003) VPW 2.0 VR.vbs` | line 42: `Const VRRoom = 3` |
| Star Wars | `Star Wars (Data East 1992) VPW 1.2.2 VR.vbs` | line 43: `Const VRRoom = 2` |
| TRON Legacy | `Disney TRON Legacy (Limited Edition) (Stern 2011) VPW 1.1 MOD VR.vbs` | line 78: `Const VRRoom = 3` |

The table above lists every changed line; most of them are also marked with a `' Quest VR override` comment in the scripts.

Not needed for VR tables that detect VR by themselves (`RenderingMode = 2`), e.g. Star Trek The Next Generation, Guns N' Roses, Starship Troopers, The Sopranos.

The table scripts belong to their respective authors (VPW and others), only the marked lines were changed.
