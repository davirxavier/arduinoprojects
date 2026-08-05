# MQTT Discovery Entity Properties

## Common (all entity types)

These properties are supported by nearly all MQTT Discovery entities.

| Property                | Description                                        |
|-------------------------|----------------------------------------------------|
| `name`                  | Friendly name shown in Home Assistant              |
| `unique_id`             | Stable unique identifier for the entity            |
| `object_id`             | Suggested entity ID                                |
| `device`                | Groups entities under one physical device          |
| `availability_topic`    | Topic indicating device availability               |
| `payload_available`     | Payload meaning "available" (default: `online`)    |
| `payload_not_available` | Payload meaning "unavailable" (default: `offline`) |
| `icon`                  | Material Design icon (`mdi:*`)                     |
| `entity_category`       | `config` or `diagnostic`                           |
| `enabled_by_default`    | Whether the entity starts enabled                  |
| `qos`                   | MQTT QoS                                           |
| `retain`                | Whether commands should be retained                |

---

## SENSOR

**Read-only value**

| Property                   | Purpose                                    |
|----------------------------|--------------------------------------------|
| `state_topic`              | Topic containing the sensor value          |
| `value_template`           | Extract value from payload                 |
| `unit_of_measurement`      | Unit (°C, %, V, etc.)                      |
| `device_class`             | Temperature, humidity, battery, etc.       |
| `state_class`              | `measurement`, `total`, `total_increasing` |
| `expire_after`             | Marks unavailable if no updates received   |
| `json_attributes_topic`    | Additional attributes                      |
| `json_attributes_template` | Extract JSON attributes                    |

---

## BINARY_SENSOR

**Read-only ON/OFF state**

| Property                | Purpose                              |
|-------------------------|--------------------------------------|
| `state_topic`           | Topic containing state               |
| `value_template`        | Extract state from payload           |
| `payload_on`            | Payload meaning ON                   |
| `payload_off`           | Payload meaning OFF                  |
| `device_class`          | Motion, door, smoke, moisture, etc.  |
| `off_delay`             | Automatically return OFF after delay |
| `expire_after`          | Marks unavailable if updates stop    |
| `json_attributes_topic` | Additional attributes                |

---

## SWITCH

**Controllable ON/OFF entity**

| Property           | Purpose                           |
|--------------------|-----------------------------------|
| `command_topic`    | Receives ON/OFF commands          |
| `state_topic`      | Reports current state             |
| `payload_on`       | Command payload for ON            |
| `payload_off`      | Command payload for OFF           |
| `state_on`         | Payload representing ON state     |
| `state_off`        | Payload representing OFF state    |
| `value_template`   | Extract state from JSON           |
| `command_template` | Format outgoing commands          |
| `optimistic`       | Assume state without confirmation |
| `retain`           | Retain command messages           |
| `device_class`     | Optional switch type              |

---

## BUTTON

**Stateless action**

| Property           | Purpose                                    |
|--------------------|--------------------------------------------|
| `command_topic`    | Topic to publish button presses            |
| `payload_press`    | Payload sent when pressed                  |
| `command_template` | Format outgoing payload                    |
| `device_class`     | Optional (restart, update, identify, etc.) |

---

## NUMBER

**Editable numeric value**

| Property              | Purpose                    |
|-----------------------|----------------------------|
| `command_topic`       | Receives new value         |
| `state_topic`         | Reports current value      |
| `value_template`      | Extract value              |
| `command_template`    | Format outgoing value      |
| `min`                 | Minimum value              |
| `max`                 | Maximum value              |
| `step`                | Increment size             |
| `mode`                | `auto`, `box`, or `slider` |
| `unit_of_measurement` | Optional unit              |
| `device_class`        | Optional number type       |

---

## TEXT

**Editable string**

| Property           | Purpose              |
|--------------------|----------------------|
| `command_topic`    | Receives text        |
| `state_topic`      | Reports current text |
| `value_template`   | Extract text         |
| `command_template` | Format outgoing text |
| `min`              | Minimum length       |
| `max`              | Maximum length       |
| `mode`             | `text` or `password` |
| `pattern`          | Regex validation     |

---

## EVENT

**Reports discrete events**

| Property              | Purpose                 |
|-----------------------|-------------------------|
| `state_topic`         | Topic publishing events |
| `event_types`         | Allowed event names     |
| `event_type_template` | Extract event type      |
| `device_class`        | Optional event class    |

---

# Summary

| Property                     | SENSOR | BINARY_SENSOR | SWITCH | BUTTON | NUMBER | TEXT | EVENT |
|------------------------------|:------:|:-------------:|:------:|:------:|:------:|:----:|:-----:|
| `state_topic`                |   ✅    |       ✅       |   ✅    |   ❌    |   ✅    |  ✅   |   ✅   |
| `command_topic`              |   ❌    |       ❌       |   ✅    |   ✅    |   ✅    |  ✅   |   ❌   |
| `payload_on` / `payload_off` |   ❌    |       ✅       |   ✅    |   ❌    |   ❌    |  ❌   |   ❌   |
| `state_on` / `state_off`     |   ❌    |       ❌       |   ✅    |   ❌    |   ❌    |  ❌   |   ❌   |
| `payload_press`              |   ❌    |       ❌       |   ❌    |   ✅    |   ❌    |  ❌   |   ❌   |
| `value_template`             |   ✅    |       ✅       |   ✅    |   ❌    |   ✅    |  ✅   |   ❌   |
| `command_template`           |   ❌    |       ❌       |   ✅    |   ✅    |   ✅    |  ✅   |   ❌   |
| `device_class`               |   ✅    |       ✅       |   ✅    |   ✅    |   ✅    |  ❌   |   ✅   |
| `unit_of_measurement`        |   ✅    |       ❌       |   ❌    |   ❌    |   ✅    |  ❌   |   ❌   |
| `state_class`                |   ✅    |       ❌       |   ❌    |   ❌    |   ❌    |  ❌   |   ❌   |
| `min` / `max` / `step`       |   ❌    |       ❌       |   ❌    |   ❌    |   ✅    |  ❌   |   ❌   |
| `mode`                       |   ❌    |       ❌       |   ❌    |   ❌    |   ✅    |  ✅   |   ❌   |
| `pattern`                    |   ❌    |       ❌       |   ❌    |   ❌    |   ❌    |  ✅   |   ❌   |
| `event_types`                |   ❌    |       ❌       |   ❌    |   ❌    |   ❌    |  ❌   |   ✅   |
| `json_attributes_topic`      |   ✅    |       ✅       |   ✅    |   ❌    |   ✅    |  ✅   |   ❌   |
| `availability_topic`         |   ✅    |       ✅       |   ✅    |   ✅    |   ✅    |  ✅   |   ✅   |