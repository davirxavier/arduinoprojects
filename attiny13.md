
| Physical Pin (DIP-8) | Port Name | MicroCore Arduino Pin | Key Functionality |
| --- | --- | --- | --- |
| **Pin 1** | `PB5` | `5` | **External RESET** / ADC0 *(Keep as reset to avoid bricking standard ISP)* |
| **Pin 2** | `PB3` | `3` | Digital I/O / ADC3 |
| **Pin 3** | `PB4` | `4` | Digital I/O / ADC2 |
| **Pin 4** | `GND` | — | Ground (0V) |
| **Pin 5** | `PB0` | `0` | **PWM (`OC0A`)** / MOSI (ISP Programming) |
| **Pin 6** | `PB1` | `1` | **PWM (`OC0B`)** / MISO (ISP Programming) / `INT0` |
| **Pin 7** | `PB2` | `2` | SCK (ISP Programming) / ADC1 |
| **Pin 8** | `VCC` | — | Power Supply (1.8V – 5.5V) |