# GTA VI Pad for Flipper Zero

Turn your Flipper Zero into a **wired Xbox 360 (XInput) controller** with a layout made for GTA VI.
The **Back** button works as a **Shift** key, so the Flipper's 6 buttons give you 10 controller inputs.

> Unofficial fan project. Not affiliated with Rockstar Games, Take-Two Interactive or Microsoft.

🇨🇿 [Česky níže](#česky)

## Controls

| Flipper button | Normal | With Shift (hold **Back**) |
|---|---|---|
| ⬆️ Up | Left stick up | **RT** |
| ⬇️ Down | Left stick down | **LT** |
| ⬅️ Left | Left stick left | **B** |
| ➡️ Right | Left stick right | **Y** |
| ⏺ OK (center) | **A** | **X** |

- **Exit the app:** tap **Back 3× quickly** without holding any other button. The Flipper goes back to its normal USB mode.
- Each button keeps the function it had when you pressed it. If you press Back + Up (RT) and release Back first, RT stays held until you release Up.
- ⚠️ Holding **Back + Left for about 5 seconds** triggers the Flipper's built-in hardware reboot. The app can't disable this.

## Download & install

1. Download **[`fap/gta6_pad.fap`](fap/gta6_pad.fap)** (click the file, then the download button).
2. Copy it to your Flipper's SD card into the `apps/USB/` folder, for example with [qFlipper](https://flipperzero.one/update).
3. On the Flipper open **Apps → USB → GTA VI Pad**.
4. Connect the Flipper by USB cable to a device that supports wired Xbox 360 controllers, for example a computer running Xbox remote play.

**Compatibility:** the prebuilt `.fap` was built for **Momentum firmware `mntm-012`** (API 87.1).
If you use different firmware or a different version and get an **"API mismatch"** error, build the app yourself (see below).

It will most likely **not** work when plugged straight into an Xbox console, because consoles only accept authenticated controllers.

## Build from source

You need Python 3.10 or newer and [ufbt](https://pypi.org/project/ufbt/).

```sh
python3 -m pip install --upgrade ufbt

# pick the SDK that matches your Flipper's firmware:
ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json   # Momentum
# ufbt update                                                               # official firmware
# ufbt update --index-url=https://up.unleashedflip.com/directory.json         # Unleashed

# connect the Flipper by USB, close qFlipper, then in this folder:
ufbt launch
```

`ufbt launch` builds the app, uploads it to the Flipper and starts it.
To only build the file, run `ufbt`. The result is in `dist/gta6_pad.fap`.

On macOS with Python from python.org, run **Install Certificates.command** (in Applications → Python 3.x) first, otherwise downloads fail with an SSL error.

## Change the layout

Edit the `map_normal` and `map_shift` tables at the top of [`gta6_pad.c`](gta6_pad.c) and run `ufbt launch` again.
Available buttons: `RoleA`, `RoleB`, `RoleX`, `RoleY`, `RoleLB`, `RoleRB`, `RoleLT`, `RoleRT`, `RoleView`, `RoleMenu`, `RoleL3`, `RoleR3`, and the left stick directions `RoleStickUp`, `RoleStickDown`, `RoleStickLeft`, `RoleStickRight`.

## Credits & license

The USB / XInput part is based on **[USB Game Controller](https://github.com/expected-ingot/flipper-xinput)** by crapbass.
This project is licensed under the **GNU GPL-3.0**. See [LICENSE](LICENSE).

---

## Česky

**GTA VI Pad** udělá z Flipperu Zero **drátový ovladač Xbox 360** s rozložením pro GTA VI. Tlačítko **Zpět** funguje jako **shift**.

| Tlačítko Flipperu | Normálně | Se shiftem (držíš Zpět) |
|---|---|---|
| Nahoru | levá páčka nahoru | **RT** |
| Dolů | levá páčka dolů | **LT** |
| Doleva | levá páčka doleva | **B** |
| Doprava | levá páčka doprava | **Y** |
| OK (prostřední) | **A** | **X** |

**Instalace**
1. Stáhni **[`fap/gta6_pad.fap`](fap/gta6_pad.fap)**.
2. Nahraj ho přes qFlipper na SD kartu Flipperu do složky `apps/USB/`.
3. Ve Flipperu spusť **Apps → USB → GTA VI Pad** a připoj Flipper kabelem.

- **Ukončení:** 3× rychle ťukni na Zpět, aniž bys držel jiné tlačítko.
- Hotový `.fap` je sestavený pro **Momentum `mntm-012`**. Na jiném firmwaru nebo jiné verzi si aplikaci sestav sám podle návodu výše.
- ⚠️ Když držíš **Zpět + Doleva** zhruba 5 sekund, Flipper se sám restartuje. Je to vestavěná funkce Flipperu a aplikace ji nemůže vypnout.

Made by Alex.
