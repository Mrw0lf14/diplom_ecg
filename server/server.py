from flask import Flask, request
import csv
import os
import time
from datetime import datetime

app = Flask(__name__)

CSV_FILE = "data.csv"

# Проверяем, есть ли файл и пуст ли он
file_exists = os.path.isfile(CSV_FILE) and os.path.getsize(CSV_FILE) > 0

# Если файла нет или он пуст, создаем и записываем заголовки
if not file_exists:
    with open(CSV_FILE, "w", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["Дата", "Время", "АЦП", "Гироскоп", "Микрофон"])  # Заголовки

@app.route("/data", methods=["POST"])
def receive_data():
    try:
        data = request.get_json()

        if not data:
            return "Пустой JSON", 400

        adc = data.get("adc", 0)
        gyro = data.get("gyro", 0)
        mic = data.get("mic", 0)

        # Форматируем дату и время
        now = datetime.now()
        date_str = now.strftime("%Y-%m-%d")  # YYYY-MM-DD
        time_str = now.strftime("%H:%M:%S")  # HH:MM:SS

        # Запись в CSV
        with open(CSV_FILE, "a", newline="") as file:
            writer = csv.writer(file)
            writer.writerow([date_str, time_str, adc, gyro, mic])

        return "OK", 200

    except Exception as e:
        print("Ошибка:", e)
        return "Ошибка сервера", 500

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
