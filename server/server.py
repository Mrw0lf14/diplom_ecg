from flask import Flask, request, send_from_directory, jsonify, render_template_string
import csv
import os
from datetime import datetime

app = Flask(__name__)

# Папка для хранения файлов
DATA_FOLDER = "data"
os.makedirs(DATA_FOLDER, exist_ok=True)

HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Сервер обработки портативных мониторов</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; }
        table { width: 80%%; margin: 20px auto; border-collapse: collapse; }
        th, td { border: 1px solid black; padding: 10px; }
        th { background-color: #f2f2f2; }
        a { text-decoration: none; padding: 5px 10px; background-color: #4CAF50; color: white; border-radius: 5px; }
        a:hover { background-color: #45a049; }
    </style>
</head>
<body>
    <h1>Сервер обработки портативных мониторов</h1>
    <table>
        <tr>
            <th>Файл</th>
            <th>Последняя загрузка</th>
            <th>Действие</th>
        </tr>
        {% for file, date in files %}
        <tr>
            <td>{{ file }}</td>
            <td>{{ date }}</td>
            <td><a href="{{ url_for('download_file', filename=file) }}">Скачать</a></td>
        </tr>
        {% endfor %}
    </table>
</body>
</html>
"""

@app.route("/")
def index():
    files_info = []
    for filename in os.listdir(DATA_FOLDER):
        file_path = os.path.join(DATA_FOLDER, filename)
        if os.path.isfile(file_path):
            last_modified = datetime.fromtimestamp(os.path.getmtime(file_path)).strftime("%Y-%m-%d %H:%M:%S")
            files_info.append((filename, last_modified))
    
    return render_template_string(HTML_TEMPLATE, files=files_info)

@app.route("/data", methods=["POST"])
def receive_data():
    try:
        data = request.get_json()
        if not data or "device_name" not in data:
            return "Ошибка: Не передано имя устройства", 400

        device_name = data["device_name"]
        filename = f"data_{device_name}.csv"
        adc = data.get("adc", 0)
        gyro = data.get("gyro", 0)
        mic = data.get("mic", 0)

        now = datetime.now()
        date_str = now.strftime("%Y-%m-%d")
        time_str = now.strftime("%H:%M:%S")

        file_path = os.path.join(DATA_FOLDER, filename)
        
        file_exists = os.path.isfile(file_path) and os.path.getsize(file_path) > 0
        with open(file_path, "a", newline="") as file:
            writer = csv.writer(file)
            if not file_exists:
                writer.writerow(["Дата", "Время", "АЦП", "Гироскоп", "Микрофон"])
            writer.writerow([date_str, time_str, adc, gyro, mic])

        return jsonify({"status": "OK", "file": filename}), 200

    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route("/download/<filename>", methods=["GET"])
def download_file(filename):
    return send_from_directory(DATA_FOLDER, filename, as_attachment=True)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
