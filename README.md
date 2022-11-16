# esp32_beacon
getting started esp32 for beacon

## beacon
- april ABTemp
- jinou

## supporting system
- vs code
- plugin: platform.io

## MQTT Pub-Sub
- mosquitto_sub -h broker.emqx.io -p 1883 -t smarthome/beacon/# -v
- mosquitto_pub -h broker.emqx.io -p 1883 -t smarthome/beacon/id/setting -m 'reboot'
- mosquitto_pub -h broker.emqx.io -p 1883 -t smarthome/beacon/id/setting -m '[interval|4]'
- mosquitto_pub -h broker.emqx.io -p 1883 -t smarthome/beacon/id/setting -m 'status'

## instruction
- https://andriantriputra.medium.com/micro-controller-esp32-beacon-ac60239c7659


