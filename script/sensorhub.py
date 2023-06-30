import smbus

DEVICE_BUS = 1
DEVICE_ADDR = 0x17

TEMP_REG = 0x01
LIGHT_REG_L = 0x02
LIGHT_REG_H = 0x03
STATUS_REG = 0x04
ON_BOARD_TEMP_REG = 0x05
ON_BOARD_HUMIDITY_REG = 0x06
ON_BOARD_SENSOR_ERROR = 0x07
BMP280_TEMP_REG = 0x08
BMP280_PRESSURE_REG_L = 0x09
BMP280_PRESSURE_REG_M = 0x0A
BMP280_PRESSURE_REG_H = 0x0B
BMP280_STATUS = 0x0C
HUMAN_DETECT = 0x0D

def readout_sensorhub(ut_str: str, filename: str):
    bus = smbus.SMBus(DEVICE_BUS)

    aReceiveBuf = []
    aReceiveBuf.append(0x00)
    for i in range(TEMP_REG, HUMAN_DETECT + 1):
        aReceiveBuf.append(bus.read_byte_data(DEVICE_ADDR, i))

    fields = ["" for _ in range(13)]
    fields[ 0] = f"{aReceiveBuf[TEMP_REG]}"
    fields[ 1] = f"{1 if aReceiveBuf[STATUS_REG] & 0x01 else 0}"
    fields[ 2] = f"{1 if aReceiveBuf[STATUS_REG] & 0x02 else 0}"
    fields[ 3] = f"{aReceiveBuf[ON_BOARD_TEMP_REG]}"
    fields[ 4] = f"{aReceiveBuf[ON_BOARD_HUMIDITY_REG]}"
    fields[ 5] = f"{aReceiveBuf[ON_BOARD_SENSOR_ERROR]}"
    fields[ 6] = f"{aReceiveBuf[BMP280_TEMP_REG]}"
    fields[ 7] = f"{aReceiveBuf[BMP280_PRESSURE_REG_L] | aReceiveBuf[BMP280_PRESSURE_REG_M] << 8 | aReceiveBuf[BMP280_PRESSURE_REG_H] << 16}"
    fields[ 8] = f"{aReceiveBuf[BMP280_STATUS]}"
    fields[ 9] = f"{aReceiveBuf[LIGHT_REG_H] << 8 | aReceiveBuf[LIGHT_REG_L]}"
    fields[10] = f"{1 if aReceiveBuf[STATUS_REG] & 0x04 else 0}"
    fields[11] = f"{1 if aReceiveBuf[STATUS_REG] & 0x08 else 0}"
    fields[12] = f"{aReceiveBuf[HUMAN_DETECT]}"

    out = open(filename, "a")
    out.write(f"{ut_str}, {', '.join(fields)}\n")
    out.close()

