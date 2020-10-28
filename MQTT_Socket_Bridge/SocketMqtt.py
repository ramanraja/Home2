# Flask MQTT - Web socket Bridge
# Test it with MQTT-FX
# To test the socket part, use:  index.html
# https://flask-mqtt.readthedocs.io/en/latest/usage.html#configure-the-mqtt-client
# https://flask-mqtt.readthedocs.io/en/latest/usage.html
# pip install flask-mqtt
# pip install flask-bootstrap
# pip install gevent
# pip install gevent-websocket
'''
Flask-MQTT is currently not suitable for the use with multiple worker instances. (1) So if you use a WSGI server like 
gevent or gunicorn make sure you only have one worker instance. (2) Make sure to disable Flasks autoreloader.
'''

#import eventlet
####eventlet.monkey_patch() # needs Linux
from flask import Flask, render_template
from flask_mqtt import Mqtt
#from flask_bootstrap import Bootstrap
from flask_socketio import SocketIO,emit,disconnect
from flask_cors import CORS


HTTPport = 5000
sub_topic = 'cmnd/raja'
pub_topic = 'stat/raja'

app = Flask(__name__)
app.config['SECRET'] = 'my-secret-key!'
app.config['TEMPLATES_AUTO_RELOAD'] = True
app.config['MQTT_BROKER_URL'] = 'localhost'  # 'broker.hivemq.com'   
app.config['MQTT_BROKER_PORT'] = 1883
app.config['MQTT_KEEPALIVE'] = 60  # in seconds
app.config['MQTT_TLS_ENABLED'] = False  

mqtt = Mqtt(app)
#socketio = SocketIO(app, cors_allowed_origins="*")  # does not send socket data from MQTT thread
#socketio = SocketIO (app, async_mode=None, cors_allowed_origins="*")  # does not send socket data from MQTT thread
#socketio = SocketIO (app, async_mode=eventlet, cors_allowed_origins="*") # does not send socket data from MQTT thread
#socketio = SocketIO (app, async_mode='threading', cors_allowed_origins="*") # this works, but by long polling
socketio = SocketIO (app, async_mode='gevent', cors_allowed_origins="*") # this works best


@app.route('/')
def index():
    return render_template('index.html')


@app.route('/ping', methods=['GET'])
def ping_em():
    print ('Poking client...')
    socketio.send ('Server pokes you!', broadcast=True)
    return ('poked and pinged him!')

    
@socketio.on('connect')
def on_connect ():
    print ('Connected to socket client.')
    print ('Subscribing to MQTT: ', sub_topic)
    mqtt.subscribe(sub_topic)  # TODO: avoid duplicate subs
    socketio.send('Welcome client!')   # broadcast=True is implied


@socketio.on('disconnect')
def on_disconnect():
    print ('Client disconnected. unsubscribing..')
    mqtt.unsubscribe_all()     # TODO: unsub only when client count is 0


@socketio.on ('client-event')  # ('publish')
def on_publish (payload):
    print ('publishing: ', payload)
    mqtt.publish (pub_topic, payload)
    socketio.send ('Published: ' +payload)
    
    
@mqtt.on_connect() # THIS IS NEVER CALLED!?  
def on_connect (client, userdata, flags, rc):
    print ('\nConnected to MQTT broker.\n')
    ###mqtt.subscribe (sub_topic) # if it works, this will be duplicate subscription
    socketio.send ('Connected to MQTT broker.')
    mqtt.publish (pub_topic, 'Hi, MQTT!')

    
@mqtt.on_message()
def on_mqtt_message(client, userdata, message):
    msg = message.payload.decode()
    print ("MQTT msg received: ", msg)
    socketio.emit('server-event', msg)
    #socketio.send ('Message dummy', broadcast=True)  #(msg)


if __name__ == '__main__':
    # disabling reloader is important to avoid duplicate messages and loops!
    socketio.run(app, host='0.0.0.0', port=5000, use_reloader=False, debug=True)
    