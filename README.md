# FALLEN SIGNAL — نسخهٔ اول

یک vertical slice سه‌بعدیِ بدون موتور بازی: حسِ نشانه‌گیرِ مرجع، چشم‌اندازی تاریک و عمیق، و موجوداتِ نورانی که از پشت سامانه عبور می‌کنند.

> نام فعلی فقط یک نامِ کاری است و هر زمان می‌توانیم عوضش کنیم.

## اجرای سریع ویندوز — بدون مرورگر و بدون نصب ابزار توسعه

فایل آمادهٔ native در [`downloads/FallenSignal-Windows-x64.zip`](downloads/FallenSignal-Windows-x64.zip) قرار دارد. برای اجرا به CMake، Visual Studio یا browser نیاز نیست؛ فقط Windows 10/11 x64 و OpenGL 3.3+ لازم است.

اگر همین repository را clone کرده‌ای، در PowerShell این را بزن:

```powershell
git pull; Expand-Archive .\downloads\FallenSignal-Windows-x64.zip -DestinationPath .\FallenSignal -Force; Start-Process .\FallenSignal\FallenSignal.exe
```

## در این نسخه چه ساخته شده است؟

- **C++20 + OpenGL 3.3 Core**؛ هیچ game engine، Three.js، Unity یا Unreal استفاده نشده است.
- **GLFW** فقط پنجره، OpenGL context و ورودی کیبورد/ماوس را فراهم می‌کند؛ تمام رندر، ماتریس‌ها، شیدرها، HUD، مدل‌های سه‌بعدی، پرتابه و منطق بازی در `src/` نوشته شده‌اند.
- یک درهٔ procedural با وسعت بیش از یک کیلومتر، مهِ سنگین لایه‌لایه، آسمان سبزِ تیره و post-process سینمایی: grain، vignette، chromatic aberration و bloom کنترل‌شده.
- HUD نشانه‌گیری مات و تیره با خط‌کش‌های ناقص، rail عمودی، chevronهای پایین‌رونده، منحنی بالستیک و تنها یک cue قرمز؛ به‌جای رابط نئونیِ بازی‌گونه.
- فرشته‌ها/سیگنال‌ها از هندسهٔ سه‌بعدیِ procedural ساخته می‌شوند: بدن حجمی، ۲۸ پرِ جداگانه در هر بال، لایهٔ پرهای پوششی و دمِ بادبزنی. هیچ عکس یا textureای برای خود فرشته در بازی نیست. مسیر پرواز نسبت به فاصله در حدود ۷ تا ۲۱ درجه بالاتر از سامانه تنظیم می‌شود، بنابراین دوربین عمداً زیرِ بال‌ها را می‌بیند و فرم پرها از زیر خوانا می‌ماند.
- اپتیک به یک برجک/سامانهٔ واقعی محدود شده است: میدان دید در حالت عادی ۱۸ درجه است، زوم تا `x2.6` می‌رود و yaw/pitch آزادِ ۳۶۰ درجه نیست. نسخهٔ آماده هنگام اجرا تمام‌صفحهٔ native باز می‌شود تا title bar و taskbar دورِ منظره دیده نشود.
- یک سیگنالِ اصلی در شروع و هر زمان قاب برای مدتی خالی شود، در فاصلهٔ نزدیکِ حدود ۱۵۵ متر داخل محور اپتیک وارد می‌شود تا بدن و پرهای سه‌بعدی واقعاً خوانا باشند. دیگر سیگنال‌ها همچنان از پشت سامانه می‌رسند و ارتفاع/فاصلهٔ متفاوت دارند.
- شلیک **hitscan نیست**: توپ سامانه گلوله‌های کروی و درخشانِ قرمز با tracer قرمز شلیک می‌کند. هر گلوله سرعت، زمان سفر و افت بالستیک دارد. fire-control فقط وقتی هدف دقیقاً lock شود، حرکت هدف و افت گلوله را برای شلیک محاسبه می‌کند؛ بیرون از lock تیر دقیقاً در امتداد اپتیک می‌رود.
- برخورد موفق بدون خشونت گرافیکی، سیگنال را به نور قرمز/آبیِ در حال پراکندن تبدیل می‌کند و بعد موج تازه‌ای از پشت صحنه می‌آید.

## کنترل‌ها

| کلید | عملکرد |
| --- | --- |
| `W` / `A` / `S` / `D` | حرکت سریع نشانه‌گیر/برجک (بالا، چپ، پایین، راست) |
| کلیدهای جهت‌دار | حرکت جزئی و دقیق نشانه‌گیر |
| `+` یا `=` / `-` | زوم‌این / زوم‌اوت اپتیک، از `x1.0` تا `x2.6` |
| کلیک چپ یا `Space` | شلیک گلولهٔ کروی قرمزِ توپ با افت بالستیک |
| ماوس | نشانه‌گیری اختیاری و آزاد |
| `R` | بازنشانی encounter و امتیاز |
| `Esc` | بار اول آزاد کردن موس؛ بار دوم بستن بازی |

اعداد HUD، از چپ به راست، range هدف، زوم، زمان تقریبی رسیدن پرتابه (صدم ثانیه) و تعداد سیگنال‌های resolve‌شده هستند. `LOCK` یا `SEARCH` و زمان پرواز در عنوان پنجره نیز به‌روز می‌شوند.

## اجرای یک‌خطی در VS Code (ویندوز)

### پیش‌نیازِ یک‌باره

1. **Visual Studio 2022 Community** یا **Build Tools 2022** را با workloadِ **Desktop development with C++** نصب کن.
2. Git باید در `PATH` باشد. اسکریپت در صورت نبودن CMake، ابتدا تلاش می‌کند آن را با `winget` نصب کند؛ در غیر این صورت CMake 3.24+ را دستی نصب کن.
3. درایور GPU باید OpenGL 3.3 یا جدیدتر را پشتیبانی کند.

سپس پوشهٔ پروژه را در VS Code باز کن، ترمینال PowerShell را باز کن و فقط این را بزن:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run.ps1
```

در اجرای اول، CMake خودش GLFW را دانلود می‌کند؛ سپس پروژه را می‌سازد و `fallen_signal.exe` را اجرا می‌کند. GLAD از قبل به‌شکل source تولیدشده در `third_party/glad` قرار دارد تا نیازی به Python یا نصب ابزار جانبی دیگر نباشد.

اگر هنوز سورس را clone نکرده‌ای، این **یک خط** هم پروژه را دانلود می‌کند و هم اجرا را آغاز می‌کند:

```powershell
git clone --branch arena/01a034f2-say --single-branch https://github.com/Xray-Co61/say.git fallen-signal; Set-Location .\fallen-signal; powershell -ExecutionPolicy Bypass -File .\scripts\run.ps1
```

اگر ترجیح می‌دهی بدون اسکریپت اجرا کنی:

```powershell
cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug
.\out\build\windows-x64-debug\Debug\fallen_signal.exe
```

## ساختار سورس

```text
src/
  main.cpp       # lifecycle پنجره و game loop
  Math.hpp       # Vec3 و ماتریس‌های دست‌نویس
  Shader.*       # کامپایل/مدیریت shaderهای OpenGL
  Mesh.*         # VAO/VBO/EBO و مش‌های procedural
  Shaders.hpp    # shader sourceها
  Game.*         # نشانه‌گیری، spawn، projectile، HUD و رندر صحنه
third_party/glad/ # loader تولیدشدهٔ OpenGL 3.3 (MIT)
scripts/run.ps1  # configure + download dependency + build + run
```

## گام‌های طبیعی بعدی

- مدل سه‌بعدی دقیق‌تر و انیمیشن اسکلتی برای فرشته‌ها؛
- صدا، رادیو/نویز، impact و موسیقی محیطی؛
- پیش‌بینی مسیر در HUD و سیستم قفل حرفه‌ای‌تر؛
- موج‌ها، مأموریت‌ها، هدف‌های متنوع و روایت؛
- post-process واقعی برای bloom، grain و depth haze.
