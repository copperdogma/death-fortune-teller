# Skull Jaw Mechanism — Through-Axle D-Shaft Drive

20251101: Created via GPT5 convo: https://chatgpt.com/g/g-p-68992f0599d081918441e6fcb4fac8a9-electronics/c/69069e96-fc2c-832a-a9a2-5fc38d97ba37

20251103: Ordered all parts.

## Dimensions
- Death skull mandible width: 85.5mm (barely touching each side)
- Jeff/Anthony skull mandible width: ~90mm (barely touching each side)


## AI Writeup of Convo

### Goal

Create a **hidden, safe, and compliant** jaw-opening mechanism that:

* Uses a single metal axle through the hinge line
* Lets the mandible open and close smoothly
* Allows full disassembly
* Hides all moving parts inside the skull

---

## ⚙️ Bill of Materials (BOM)

| Qty | Item                              | Specification / Notes                                                        |
| --- | --------------------------------- | ---------------------------------------------------------------------------- |
| 1   | **Axle (main shaft)**             | 3 mm × (straight-line skull width + 6 mm) stainless or music-wire rod        |
| 2   | **Flanged bushings**              | 3 mm ID × 5–6 mm OD brass or sintered-bronze, one per skull wall             |
| 1   | **Mini metal-gear servo**         | 3–4 kg·cm torque (e.g., MG90S or similar)                                    |
| 1   | **Crank arm**                     | Aluminum or printed lever, 10–15 mm radius, M3 clamp or set-screw hub        |
| 1   | **Light torsion spring**          | 3 mm ID, 6–8 mm OD, 0.6–0.8 mm wire, 2–3 coils                               |
| 2   | **3 mm shaft collars**            | With M3 grub screws, for axial retention                                     |
| 1   | **Series-elastic link**           | Short 5–10 mm section of silicone fuel tube with 1 mm piano-wire Z-bent ends |
| 1   | **Servo saver horn** *(optional)* | RC-car type, matches your servo spline                                       |
| 1   | **Extension spring (alt.)**       | 20–30 mm long, light pull (if you prefer spring-close instead of torsion)    |
| 1   | **Mounting screw + washer**       | M2–M3 screw for anchoring spring to skull interior                           |
| —   | Epoxy (slow-set)                  | For fixing bushings and D-sleeves                                            |
| —   | Superglue or hot-melt             | For lightweight cable routing or small tacks                                 |
| —   | Paint/marker                      | For marking alignment lines                                                  |

---

## 🧰 Tools Needed

* Small vise or clamps
* Fine flat file or your **EZE-Lap diamond needle files**
* Hacksaw or rotary tool (for cutting the rod)
* 1.5 mm hex key (for collars)
* Small drill (for hinge cleanup or spring-anchor pilot)
* Calipers or ruler
* Screwdriver set

---

## 🪛 Construction Steps

### 1. Prepare the Skull Hinge

1. Pop the mandible out.
2. Drill clean 3 mm holes straight through both hinge posts on the skull so they’re perfectly coaxial.
3. Test-fit the rod through both sides; adjust until it spins freely.
4. Epoxy the **flanged bushings** into the skull from the inside—flange inward.

### 2. Prepare the Mandible Hubs

1. Enlarge each jaw hinge pocket just enough for a 3 mm-ID sleeve.
2. On **one** side (drive side), epoxy in a **D-bore sleeve** (printed or brass) keyed to match your D-shaft.
3. On the **other** side, epoxy in a **round 3 mm sleeve** for free rotation.
4. Let cure fully.

### 3. Make the D-Shaft Flat

1. Clamp the 3 mm rod in a vise.
2. Mark a 15 mm section near one end.
3. Using your **diamond file**, remove ~0.5 mm depth to create a flat.
4. Test fit in the D-sleeve hub—it should slide snugly but not bind.
5. Deburr the edges with a fine file.

*(If using a crank arm that also needs a flat, mark another 10 mm section along the shaft where it will sit and file a second flat.)*

### 4. Fit the Axle

1. Slide the rod through one skull bushing → through the mandible hubs → through the other bushing.
2. The jaw should rotate smoothly.
3. Install **shaft collars** just inside each bushing to keep the axle from sliding left/right.
4. Tighten collar grub screws lightly.

### 5. Add the Crank Arm and Spring

1. Mount the **crank arm** on the inner flat of the axle and tighten its set screw.
2. Slip the **torsion spring** around the axle beside the crank arm.

   * One leg hooks onto the crank arm (rotating part).
   * The other leg hooks to a small screw or eyelet anchored in the skull interior.
   * Preload it slightly (~15° twist) so the jaw wants to close.
3. Check that it closes gently without binding.

*(If you prefer an extension spring: anchor one end to the mandible and the other to a skull screw so it holds the mouth shut.)*

### 6. Link the Servo

1. Mount the servo on an internal bracket near the hinge so the horn swings roughly parallel to the crank arm’s plane.
2. Connect the crank arm to the servo horn using the **silicone-tube link** (one Z-bend each side).

   * The tube adds flex and prevents hard stalls.
3. If using a servo-saver horn, install that instead of a rigid one.

### 7. Test & Tune

1. Power the servo and move it slowly through its range.
2. Adjust linkage length so:

   * Mouth closed = servo near one endpoint but not straining.
   * Mouth open = comfortable, no binding.
3. Add soft mechanical stops (rubber pads) if needed.
4. Program a short ease-in/ease-out motion in firmware for smooth action.

---

## 🧩 Assembly Layout (text sketch)

```
[skull wall] [bushing][collar]--shaft-->[D-hub in jaw]---[round hub in jaw]<--shaft--[collar][bushing][skull wall]
                              |
                              +-- crank arm + torsion spring
                                    |
                                 silicone link
                                    |
                                  servo
```

---

## 🧼 Maintenance & Disassembly

* To remove the jaw: flex the mandible open 1–2 cm to pop the D-sleeve off the shaft ends.
* To remove the entire shaft: loosen the collars and crank-arm screw, unhook the spring, then slide the shaft out.
* Re-lubricate bushings annually with a touch of light machine oil.

---

## ✅ Result

* Hidden, safe, and quiet mechanism
* Full compliance in both directions
* Removable jaw for service
* Zero risk of servo mount damage
* Realistic motion and “snap-close” feel from the spring

