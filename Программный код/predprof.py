import os
import time
from datetime import datetime

import pandas as pd
import serial

# Установите параметры последовательного порта
arduino = serial.Serial(port='COM22', baudrate=115200, timeout=1)

# Задержка для установления соединения
time.sleep(2)

print("Соединение с Arduino установлено!")


def get_dataframe(filename):
    return pd.read_excel(filename, engine="openpyxl") if os.path.exists(filename) else pd.DataFrame(
        columns=["Фамилия", "UID номер", "Срок окончания доступа"])


def save_dataframe(df, filename):
    df.to_excel(filename, index=False, engine="openpyxl")


def add_rfid_key(name, uid, expiry_date, filename="access_control.xlsx"):
    df = get_dataframe(filename)
    df.loc[len(df)] = [name, uid, expiry_date]
    save_dataframe(df, filename)


def check_rfid_access(uid, filename="access_control.xlsx"):
    df = get_dataframe(filename)
    record = df[df["UID номер"] == uid]
    return not record.empty and datetime.now() <= datetime.strptime(record.iloc[0]["Срок окончания доступа"],
                                                                    "%Y-%m-%d")


def revoke_rfid_access(uid, filename="access_control.xlsx"):
    df = get_dataframe(filename)
    df = df[df["UID номер"] != uid]
    save_dataframe(df, filename)


def send_data(data):
    print(f"Отправляю: {data}")
    arduino.write(f"{data}\n".encode())  # Отправка строки, закодированной в байты


def check_code():
    data = arduino.readline().decode().strip()  # Читаем строку и декодируем её
    if data[:2] == "cd":
        if check_rfid_access(data[2:]):
            send_data("TRUE")
        else:
            send_data("FALSE")


if __name__ == "__main__":
    # add_rfid_key("1", "112233445566778899aabbccddeeff0", "2025-02-15")
    # add_rfid_key("2", "f1e2d3c4b5a69788796a5b4c3d2e1f0", "2025-03-01")
    # add_rfid_key("3", "0000000000000000", "2025-02-05")
    # add_rfid_key("4", "5a6b7c8d9eafb0c1d2e3f4516273849", "2025-04-10")

    print(get_dataframe("access_control.xlsx"))
    try:
        while True:
            if arduino.in_waiting > 0:  # Проверяем, есть ли данные в буфере
                check_code()
    except KeyboardInterrupt:
        print("Программа завершена.")
        arduino.close()