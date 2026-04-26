# S3-Go!-radio

На создания этой программы меня вдохновил замечательный проект Ёрадио https://4pda.to/stat/go?u=https%3A%2F%2Fgithub.com%2Fe2002%2Fyoradio%2F&e=101867730&f=https%3A%2F%2F4pda.to%2Fforum%2Findex.php%3Fshowtopic%3D1010378%26st%3D1560
Основная цель этого проекта — создание интернет-радио с графическим оформлением (фон фото). Он построен на чипе ESP32S3 N16R8 и экране ST7796(320*480)//другие экраны будут добавляться по мере запроса, библиотека позволяет добавить быстро//


Характеристики радио:
- 5 рабочих екранов (часы 1, часы 2, вуметр 1-3, метаданные)
- неограниченное количество плейлистов, ограничено только памятью платы.
- читает все форматы mp3, aac, flac...
- смена фона, настройка положения и цвета текста с веба без перезагрузки
- настройка размера, цвета, логики движения и положения стрелок вуметра с веба
- загрузка и удаление файлов с веб
- настройка пинов платы, выбор типа дисплея  в файле config.h
-  управление 5 кнопок, энкодер ( 1 или 2), пульт 5 кнопок (коды вручную прописать в config.h)
-  Управление адресной led лентой типа 2812 (пин 48)
-  возможность сохранять свои темы и делиться с другими
-  автоматическое создание плейлиста favorit.csv и возможность добавлять в него потоки нажатием "+" в плейлисте
-  темная/светлая тема


-  - для потоков Flac обновить файлы как написано в инструкции здесь :
https://4pda.to/forum/index.php?showtopic=1010378&view=findpost&p=125839228

Путь к папке для VSC 
C\Users\User\ .platformio\packages\framework-arduinoespressif32-libs\esp32s3\lib\

Если проблемы будут (а они как обычно есть), ссылка в телеграм канал(новая ссылка)

 https://t.me/s3go_internetradio

Комплектующие:
- esp32s3 n16r8 44 pin
- https://www.aliexpress.com/item/1005005592730189.html
- dac5102/5100
- https://www.aliexpress.com/item/1005006104038963.html
- ![photo_2025-11-07_15-34-59](https://github.com/user-attachments/assets/2cce3f1e-06b2-4b7a-8eae-dc9c1c50d812)
- https://www.aliexpress.com/item/1005008144198547.html
- пины подключения 5100 (со встроенным унч)
- ![5100](https://github.com/user-attachments/assets/29752642-1eb8-4b01-8574-87e5249ebbde)
- spi tft ST7796 или другой
- кнопки, энкодеры, пульт


.................................................................................................................
.................................................................................................................


Прошивка с помощью программы VSC.

https://code.visualstudio.com/
Запустить программу, установить PlatformIO IDE.
Установить https://git-scm.com/install/windows

Скачать архив, распаковать в корень диска. Путь к папке (и имя папки) не должен содержать кириллицу.
Открыть папку проекта в программе VSC.
Подождать скачивание библиотек и ядра.
Прошить код.
- следующий пункт можно не делать, можно залить файлы через веб, после установки сети

Прошить файловую систему (data). нажать "upload filesystem image". В папку data можно положить плейлисты и фото (имя не должно написано кирилицей).

<img src="https://github.com/user-attachments/assets/c880c923-1d6f-4cac-8eb3-e780807a8c41" width="25%"/>

При первом запуске подключится к wi-fi платы имя сети "S3-Go!-light-Setup", ввести адрес http://192.168.4.1, прописать имя и пароль вашей сети.

<img src="https://github.com/user-attachments/assets/a50fae03-4f8d-49fb-80dc-2f2b9dc3c7cc" width="25%"/>
<img src="https://github.com/user-attachments/assets/7b07264d-4a27-44dd-9381-0d08c0295fd9" width="25%"/>



Библиотеки:

  https://github.com/Bodmer/TJpg_Decoder.git
  
  https://github.com/schreibfaul1/ESP32-audioI2S @ ^3.4.5
  
  https://github.com/moononournation/Arduino_GFX.git
  
  bblanchon/ArduinoJson @ ^7.4.2
  
  https://github.com/Arduino-IRremote/Arduino-IRremote.git
  
  adafruit/Adafruit NeoPixel @ ^1.12.0

Фото:

https://github.com/user-attachments/assets/048a29b1-905f-432f-9c62-cb85d12a1eee

<img src="https://github.com/user-attachments/assets/a839ecb1-c02e-46b1-a70b-7a8718b49b34" width="25%"/>
<img src="https://github.com/user-attachments/assets/4b673357-a6c2-4fbe-9c64-25f2e437e369" width="25%"/>
<img src="https://github.com/user-attachments/assets/2f54f73e-4fb3-40d6-aec2-c51426016941" width="25%"/>
<img src="https://github.com/user-attachments/assets/8c40a43a-6a98-4bf8-a2d5-7fa698b6132e" width="25%"/>
<img src="https://github.com/user-attachments/assets/a18ccc0e-9de5-4f83-abfb-bbf644187b30" width="25%"/>

Скриншоты веб страницы:


Фото на фон создавать соответственно разрешению экрана.
При сохранении фото в фотошопе выбрать настройку "базовый оптимизированный"

![photoshop](https://github.com/user-attachments/assets/e95ff6d8-e2c6-4c6e-b7c7-53681563412b)
