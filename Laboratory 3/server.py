import io
from flask import Flask, send_file
import os.path

app = Flask(__name__)

@app.route('/firmware.bin')
def firm():
    with open(".pio\\build\\esp-wrover-kit\\firmware.bin", 'rb') as bites:
        print(bites)
        return send_file(
                     io.BytesIO(bites.read()),
                     mimetype='application/octet-stream'
               )

@app.route("/")
def hello():
    return "Hello World!"


@app.route('/version')
def version():
    with open("versioning") as f:
        v = f.readline()
        print(v)
        return v

if __name__ == '__main__':
    app.run(host='0.0.0.0', ssl_context=('ca_cert.pem', 'ca_key.pem'), debug=True)