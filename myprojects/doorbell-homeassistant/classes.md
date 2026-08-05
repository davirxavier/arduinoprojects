# Home Assistant MQTT Device Classes

`device_class` tells Home Assistant what an entity represents.  
It affects:
- icon selection
- UI presentation
- automation behavior
- semantic meaning

---

# SENSOR device_class values

Component:

```
homeassistant/sensor/<id>/config
```

| Device Class | Description | Typical Unit |
|--------------|-------------|--------------|
| `apparent_power` | Apparent electrical power | VA |
| `aqi` | Air quality index | AQI |
| `atmospheric_pressure` | Atmospheric pressure | hPa |
| `battery` | Battery level | % |
| `blood_glucose_concentration` | Blood glucose level | mg/dL or mmol/L |
| `carbon_dioxide` | CO₂ concentration | ppm |
| `carbon_monoxide` | CO concentration | ppm |
| `current` | Electrical current | A |
| `data_rate` | Network data rate | bit/s |
| `data_size` | Data size | bytes |
| `date` | Date value | date |
| `distance` | Distance | m |
| `duration` | Duration | s |
| `energy` | Energy consumption | kWh |
| `energy_distance` | Energy efficiency | Wh/km |
| `enum` | Enumeration value | - |
| `frequency` | Frequency | Hz |
| `gas` | Gas consumption | m³ |
| `humidity` | Humidity | % |
| `illuminance` | Light level | lx |
| `irradiance` | Solar radiation | W/m² |
| `moisture` | Soil moisture | % |
| `monetary` | Currency value | currency |
| `nitrogen_dioxide` | NO₂ level | µg/m³ |
| `nitrogen_monoxide` | NO level | µg/m³ |
| `nitrous_oxide` | N₂O level | ppm |
| `ozone` | O₃ level | µg/m³ |
| `ph` | pH level | pH |
| `pm1` | Particulate matter PM1 | µg/m³ |
| `pm10` | Particulate matter PM10 | µg/m³ |
| `pm25` | Particulate matter PM2.5 | µg/m³ |
| `power` | Power usage | W |
| `power_factor` | Power factor | % |
| `precipitation` | Rainfall | mm |
| `precipitation_intensity` | Rain rate | mm/h |
| `pressure` | Pressure | Pa |
| `reactive_power` | Reactive power | var |
| `signal_strength` | Signal strength | dBm |
| `sound_pressure` | Sound level | dB |
| `speed` | Speed | m/s |
| `sulphur_dioxide` | SO₂ level | µg/m³ |
| `temperature` | Temperature | °C |
| `timestamp` | Timestamp | datetime |
| `volatile_organic_compounds` | VOC level | ppm |
| `volatile_organic_compounds_parts` | VOC level | ppb |
| `voltage` | Electrical voltage | V |
| `volume` | Volume | L |
| `volume_storage` | Stored volume | L |
| `water` | Water consumption | L |
| `weight` | Weight | kg |
| `wind_direction` | Wind direction | ° |
| `wind_speed` | Wind speed | m/s |

---

# BINARY_SENSOR device_class values

Component:

```
homeassistant/binary_sensor/<id>/config
```

| Device Class | Description |
|--------------|-------------|
| `battery` | Battery low state |
| `battery_charging` | Battery charging state |
| `carbon_monoxide` | CO detected |
| `cold` | Cold state |
| `connectivity` | Network connectivity |
| `door` | Door open/closed |
| `garage_door` | Garage door state |
| `gas` | Gas detected |
| `heat` | Heating state |
| `light` | Light detected |
| `lock` | Lock state |
| `moisture` | Moisture detected |
| `motion` | Motion detected |
| `moving` | Device movement |
| `occupancy` | Occupancy detected |
| `opening` | Opening detected |
| `plug` | Plug state |
| `power` | Power state |
| `presence` | Presence detected |
| `problem` | Problem state |
| `running` | Running state |
| `safety` | Safety state |
| `smoke` | Smoke detected |
| `sound` | Sound detected |
| `tamper` | Tamper detected |
| `update` | Update available |
| `vibration` | Vibration detected |
| `window` | Window open/closed |

---

# SWITCH device_class values

Component:

```
homeassistant/switch/<id>/config
```

| Device Class | Description |
|--------------|-------------|
| `config` | Configuration switch |
| `outlet` | Electrical outlet |
| `switch` | Generic switch |

---

# BUTTON device_class values

Component:

```
homeassistant/button/<id>/config
```

| Device Class | Description |
|--------------|-------------|
| `identify` | Identify device |
| `restart` | Restart device |
| `update` | Trigger update |

---

# NUMBER device_class values

Component:

```
homeassistant/number/<id>/config
```

| Device Class | Description |
|--------------|-------------|
| `apparent_power` | Apparent power |
| `aqi` | Air quality index |
| `atmospheric_pressure` | Pressure |
| `battery` | Battery percentage |
| `carbon_dioxide` | CO₂ |
| `carbon_monoxide` | CO |
| `current` | Electrical current |
| `data_rate` | Network speed |
| `distance` | Distance |
| `duration` | Time duration |
| `energy` | Energy |
| `frequency` | Frequency |
| `gas` | Gas |
| `humidity` | Humidity |
| `illuminance` | Light level |
| `moisture` | Moisture |
| `power` | Power |
| `pressure` | Pressure |
| `signal_strength` | Signal strength |
| `temperature` | Temperature |
| `voltage` | Voltage |
| `volume` | Volume |
| `weight` | Weight |

---

# TEXT device_class values

Component:

```
homeassistant/text/<id>/config
```

| Device Class | Description |
|--------------|-------------|
| `date` | Date input |
| `datetime` | Date and time input |
| `time` | Time input |

---

# EVENT device_class values

Component:

```
homeassistant/event/<id>/config
```

| Device Class | Description |
|--------------|-------------|
| `button` | Button press event |
| `doorbell` | Doorbell event |
| `motion` | Motion event |
| `none` | Generic event |

---

# Types without device_class

Some entity types do not normally use `device_class`:

| Entity Type | Reason |
|-------------|--------|
| `BUTTON` | Usually action-based |
| `EVENT` | Usually defined by event types |
| `TEXT` | Usually free-form input |

---

# Recommended generic defaults

| Entity Type | Default |
|-------------|---------|
| `SENSOR` | Leave unset unless meaning is known |
| `BINARY_SENSOR` | `problem`, `connectivity`, or unset |
| `SWITCH` | `switch` |
| `BUTTON` | `button` |
| `NUMBER` | Leave unset unless meaning is known |
| `TEXT` | Leave unset |
| `EVENT` | Leave unset |
