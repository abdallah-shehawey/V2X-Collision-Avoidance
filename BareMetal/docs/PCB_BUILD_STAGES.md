# V2X Car PCB — Stage-by-Stage Build Guide (Altium)

Carrier board. Nothing main is soldered: STM32 / ESP32-S3 / MPU9250 plug into
headers; L298N sits off-board wired by cable; Ultrasonics & RPi are brought out as
**parallel male-pin + female-header** for flexible mounting.

Build the schematic **one block at a time** in the order below. Each block lists:
الهدف · التوصيلات (nets) · المكتبات المطلوبة · إزاي تلاقيها وتحمّلها · ملاحظات.

---

## Where to get Altium libraries (عام لكل المراحل)

| المصدر | الرابط | بياخد منه إيه | إزاي |
|---|---|---|---|
| **SamacSys / Component Search Engine** | componentsearchengine.com | أي IC / موديول / كونيكتور | اعمل حساب مجاني + ثبّت **Altium plugin** → Search part → Place (symbol+footprint+3D) |
| **SnapEDA** | snapeda.com | dev-boards (Nucleo/ESP/HC-SR04/MPU) | Search → Download → Altium Designer format → import |
| **Ultra Librarian** | ultralibrarian.com | بديل للـ dev-boards | Search → Export → Altium |
| **Espressif official** | github.com/espressif (KiCad/Altium) | ESP32-S3 devkit | حمّل اللي Altium أو حوّل |
| **Altium built-in** | Manufacturer Part Search panel | passives + headers + كونيكتورات قياسية | جوّه البرنامج مباشرة |

> أسرع طريقة: ثبّت **SamacSys Altium plugin** (من componentsearchengine.com) — بيدخل
> جوّه Altium وبتسحب أي قطعة بضغطة. والـ dev-boards هاتها من **SnapEDA**.

---

# STAGE 1 — POWER BLOCK (الأساس — ابدأ بيه)

### الهدف
توزيع الكهربا بأمان ومن غير ضوضاء موتور تأثر على الحساسات.

### التوصيلات (nets)
```
VBAT (2S LiPo ~7.4V)  ──► VMOT  (يروح L298N مباشرة، دومين الموتور)
VBAT  ──► [BUCK 7.4→5V] ──► +5V  ──► Nucleo 5V , HC-SR04 ×6 , ESP 5V
+3V3 (من Nucleo) ──► MPU9250
GND  ──► STAR GROUND (نقطة واحدة تجمع كل الأرضيات)
```
- الراسبيري **مش بياخد كهربا من البوردة** — بس GND + UART.

### المكونات
| المكوّن | العدد | الوظيفة |
|---|---|---|
| Screw terminal 2-pin (5.08mm) | 1 | دخل البطارية VBAT |
| Buck module LM2596 (أو MP1584) ≥2A | 1 | 7.4V→5V |
| Slide switch SPDT | 1 | مفتاح رئيسي (اختياري) |
| Cap إلكتروليتي 1000µF/16V | 1 | على VMOT (اندفاع الموتور) |
| Cap إلكتروليتي 470µF/10V | 1 | على +5V |
| Cap سيراميك 100nF + 10µF | عدة | decoupling |
| Fuse holder + fuse 2-3A | 1 | حماية (اختياري) |

### المكتبات المطلوبة + إزاي تجيبها
| القطعة | المصدر | كلمة البحث |
|---|---|---|
| Screw terminal 2P 5.08 | SamacSys | `screw terminal 2 5.08` (مثلاً Phoenix `1935161`) |
| LM2596 module | ضعه كـ **1×4 header** (IN+ / IN- / OUT+ / OUT-) من Altium Misc Connectors؛ أو IC `LM2596S-5.0` من SamacSys |
| Slide switch SPDT | SamacSys | `slide switch SPDT` |
| Caps / Resistors / Fuse | Altium built-in | `Miscellaneous Devices.IntLib` |

### ملاحظات حرجة
- **Star ground:** نقطة واحدة يلتقي فيها: بطارية GND + موتور GND + 5V GND + logic GND + سلك GND بتاع الراسبيري. تيار رجوع الموتور يرجع للنقطة دي مباشرة، ميشاركش مسار أرضي الحساسات.
- افصل دومين الموتور (VMOT + L298N) في ركن لوحده.

---

# STAGE 2 — MCU BLOCK (Nucleo-F446RE)

### الهدف
قاعدة headers تركّب فيها الـ Nucleo (تقدر تشيلها).

### التوصيلات
الـ Nucleo بيطلّع كل الـ GPIO على الـ **Morpho CN7 + CN10**. هتحط هيدرز أنثى تطابقهم،
وتوصّل منهم الـ nets لباقي البلوكات. (الجدول الكامل في آخر الملف Section "PIN MAP".)

### المكونات
| المكوّن | العدد |
|---|---|
| Female header 2×19 (2.54mm) — Morpho | 2 |
| (اختياري) Female header للـ Arduino pins | حسب الحاجة |

### المكتبات + إزاي تجيبها
| القطعة | المصدر | كلمة البحث |
|---|---|---|
| Nucleo-F446RE footprint | **SnapEDA** | `Nucleo-F446RE` أو `Nucleo-64` |
| بديل: هيدر 2×19 | Altium built-in | `Header 19X2` من Misc Connectors |

### ملاحظات
- **أكّد عدد بنات الـ Morpho (2×19) من الـ mechanical drawing** بتاع لوحتك قبل ما تثبّت.
- اطلّع SWD (PA13/PA14) + PA2/PA3 (USART2 debug) على هيدر صغير.

---

# STAGE 3 — FEEDBACK BLOCK (5 LED + Buzzer) ⭐ابدأ اختبار بيه

### الهدف
أبسط بلوك — مخارج بصرية وصوتية. سهل تتأكد إنه شغّال.

### التوصيلات (nets → STM32)
```
LED_FR  → PC0      LED_FL  → PC1
LED_BR  → PC2      LED_BL  → PC3
LED_INT → PC7      BUZZER  → PC4
```
كل LED: STM32 pin → 330Ω → LED → GND. (active-high)
الـ Buzzer: لو سحبه > 8mA، حط ترانزستور NPN + 1kΩ على الـ base.

### المكونات
| المكوّن | العدد |
|---|---|
| LED (3/5mm أو 0805) | 5 |
| Resistor 330Ω | 5 |
| Active buzzer | 1 |
| NPN (2N2222/BC547) + 1kΩ + diode | 1 (لو البزر قوي) |

### المكتبات + إزاي تجيبها
| القطعة | المصدر | كلمة البحث |
|---|---|---|
| LED / Resistor | Altium built-in | `Miscellaneous Devices.IntLib` |
| Active buzzer | SamacSys | `magnetic buzzer` أو موديل البزر بتاعك |
| NPN transistor | Altium/SamacSys | `2N2222` / `BC547` |

---

# STAGE 4 — ULTRASONICS BLOCK (×6) — الأهم

### الهدف
6 حساسات، كل واحد بـ **pin متوازي + header** + خيار divider.

### التوصيلات (nets → STM32)
| الحساس | الموقع | TRIG | ECHO |
|---|---|---|---|
| US1 | Front-Left   | PB0  | PA15 |
| US2 | Front-Center | PB1  | PB3 |
| US3 | Front-Right  | PB2  | PB4 |
| US4 | Back-Left    | PB12 | PB5 |
| US5 | Back-Center  | PB13 | PC8 |
| US6 | Back-Right   | PB14 | PC9 |

**Dual-access لكل حساس:** نفس الـ 4 nets (VCC/TRIG/ECHO/GND) تروح لـ **هيدر ذكر 1×4**
**و** **هيدر أنثى 1×4** جنبه → يا تركّب الحساس مباشرة على الذكر، يا تاخد كابل من الأنثى
لو عايز ترفعه/تميله بزاوية.

**Echo level (5V→3.3V):** بنات الـ echo (PA15,PB3,PB4,PB5,PC8,PC9) كلها **5V-tolerant**
على الـ F446 (شغّال على الـ Nucleo فعلاً)، بس على PCB حط **footprint divider اختياري**:
R1=1kΩ (echo→pin) + R2=2kΩ (pin→GND). تقدر تركّب R1=0Ω وتسيب R2 فاضي لو عايز direct.

### المكونات
| المكوّن | العدد |
|---|---|
| HC-SR04 | 6 |
| Male header 1×4 | 6 |
| Female header 1×4 | 6 |
| Divider R 1k/2k (0603) | 6 أزواج |
| Cap 100nF على VCC كل حساس | 6 |

### المكتبات + إزاي تجيبها
| القطعة | المصدر | كلمة البحث |
|---|---|---|
| HC-SR04 | **SnapEDA** | `HC-SR04` |
| Headers 1×4 | Altium built-in | `Header 4` / `Receptacle 4` |
| R / C | Altium built-in | Misc Devices |

### ملاحظات
- خلي مسارات الـ echo قصيرة وبعيدة عن مسارات الموتور (الضوضاء = قراءة 400).
- 100nF على VCC كل حساس بيمنع الـ glitches.

---

# STAGE 5 — IMU BLOCK (MPU9250 / SPI1)

### الهدف
هيدر تركّب فيه الـ MPU9250 (ثابت، قابل للفك).

### التوصيلات (nets → STM32)
```
MPU_SCK  → PA5      MPU_MOSI → PA7
MPU_MISO → PA6      MPU_CS   → PA4
VCC → +3V3          GND → GND
```
| بنّة الموديول | Net | STM32 |
|---|---|---|
| SCL/SCK | MPU_SCK | PA5 |
| SDA/SDI | MPU_MOSI | PA7 |
| ADO/SDO | MPU_MISO | PA6 |
| NCS | MPU_CS | PA4 |

### المكونات
| المكوّن | العدد |
|---|---|
| Female header 1×8 | 1 |
| Cap 100nF + 10µF | 1 لكل |

### المكتبات + إزاي تجيبها
| القطعة | المصدر | كلمة البحث |
|---|---|---|
| MPU9250 / GY-91 module | **SnapEDA** | `MPU9250` / `GY-91` |
| بديل: هيدر 1×8 | Altium built-in | `Header 8` |

### ملاحظات
- 3.3V فقط (مش 5V).
- INT pin اختياري → ممكن توصّله لـ GPIO فاضي مستقبلاً.

---

# STAGE 6 — ESP32-S3 BLOCK (V2X / USART1)

### الهدف
قاعدة headers للـ ESP32-S3 + لينك UART مع الـ STM.

### التوصيلات (cross-over!)
```
STM PA9  (ESP_TXD) ──► ESP RX0     ← متقاطعة
STM PA10 (ESP_RXD) ◄── ESP TX0
+5V → ESP 5V        GND → GND
```

### المكونات
| المكوّن | العدد |
|---|---|
| Female header يطابق DevKitC-1 (≈2×22) | 2 |
| Cap 100nF | 1-2 |

### المكتبات + إزاي تجيبها
| القطعة | المصدر | كلمة البحث |
|---|---|---|
| ESP32-S3-DevKitC-1 | **Espressif official** أو **SnapEDA** | `ESP32-S3-DevKitC-1` |

### ملاحظات
- **أكّد عدد بنات الـ devkit بتاعك** (الفاريانت بيختلف).
- TX↔RX متقاطعة + GND مشترك إجباري.
- الـ baud لازم 115200 على الطرفين (زي الـ firmware).

---

# STAGE 7 — MOTOR DRIVER BLOCK (L298N — قاعدة بأسلاك)

### الهدف
الـ L298N يتحط على قاعدة ويتوصّل بكابلات (مش ملحوم).

### التوصيلات (nets → STM32)
```
MOT_R_EN  → PA8     MOT_R_IN1 → PC5    MOT_R_IN2 → PC6
MOT_L_EN  → PA11    MOT_L_IN3 → PB10   MOT_L_IN4 → PB15
VMOT → بطارية+      GND → star
```

### المكونات
| المكوّن | العدد |
|---|---|
| Male/Female header 1×6 (control) | 1 |
| Screw terminal للموتور/الباور | حسب الحاجة |
| Cap 1000µF على VMOT | 1 |

### المكتبات + إزاي تجيبها
| القطعة | المصدر | كلمة البحث |
|---|---|---|
| L298N module | SamacSys/SnapEDA | `L298N module` (أو اعمل هيدر 1×6 بسيط) |
| Screw terminals | SamacSys | `screw terminal` |

### ملاحظات
- الموتور دومين ضوضاء → بعّده عن الحساسات، والـ GND يرجع لنقطة الـ star.

---

# STAGE 8 — RASPBERRY PI 5 INTERFACE (pin + header)

### الهدف
3 أسلاك بس للراسبيري: TX, RX, GND. (الراسبيري بكهرباه الخاصة)

### التوصيلات
```
STM PA0 (RPI_TXD) ──► RPi GPIO15/RXD (pin 10)   ← متقاطعة
STM PA1 (RPI_RXD) ◄── RPi GPIO14/TXD (pin 8)
GND ──► RPi GND (pin 6)
```
- 3.3V الطرفين → **مفيش level shift**.
- اطلّعها **male pin + female header** متوازيين (زي الحساسات).

### المكونات + المكتبات
| القطعة | المصدر | كلمة البحث |
|---|---|---|
| Male + Female header 1×3 | Altium built-in | `Header 3` / `Receptacle 3` |

### ملاحظات
- **ممنوع** تشارك 5V مع الراسبيري (لا تديها ولا تاخد منها).
- GND مشترك إجباري عشان الـ UART يشتغل.

---

# STAGE 9 — DEBUG / MISC

| الإشارة | Pin | الغرض |
|---|---|---|
| SWDIO / SWCLK | PA13 / PA14 | برمجة/debug |
| USART2 (VCP) | PA2 / PA3 | serial debug |

اطلّعهم على هيدر صغير 1×4-5. (Altium `Header` built-in)

---

## ترتيب التنفيذ المقترح (schematic ثم layout)

```
1. POWER  → 2. MCU  → 3. FEEDBACK  → 4. ULTRASONICS
→ 5. IMU  → 6. ESP  → 7. MOTOR  → 8. RPi  → 9. DEBUG
```
كل مرحلة: ارسم الـ schematic block → سمّي الـ nets بالأسماء اللي فوق → annotate.
آخر خطوة: layout بنفس تقسيم الـ blocks (الموتور في ركن، الحساسات بعيد عنه).

---

## PIN MAP الكامل (مرجع — من firmware `Src/System.c`)

| Net | STM32 | Net | STM32 |
|---|---|---|---|
| MPU_SCK | PA5 | US1_TRIG/ECHO | PB0 / PA15 |
| MPU_MISO | PA6 | US2_TRIG/ECHO | PB1 / PB3 |
| MPU_MOSI | PA7 | US3_TRIG/ECHO | PB2 / PB4 |
| MPU_CS | PA4 | US4_TRIG/ECHO | PB12 / PB5 |
| ESP_TXD | PA9 | US5_TRIG/ECHO | PB13 / PC8 |
| ESP_RXD | PA10 | US6_TRIG/ECHO | PB14 / PC9 |
| RPI_TXD | PA0 | LED_FR/FL | PC0 / PC1 |
| RPI_RXD | PA1 | LED_BR/BL | PC2 / PC3 |
| MOT_R_EN | PA8 | LED_INT | PC7 |
| MOT_R_IN1/IN2 | PC5 / PC6 | BUZZER | PC4 |
| MOT_L_EN | PA11 | MOT_L_IN3/IN4 | PB10 / PB15 |

*لو غيّرت أي pin في الـ firmware، حدّث الجدول ده.*