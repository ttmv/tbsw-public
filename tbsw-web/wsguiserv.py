import sys
sys.path.append('/home/tbuser/koodit/tbsw-web/venv/lib/python3.5/site-packages')

from flask import Flask, render_template, redirect, url_for
from flask_socketio import SocketIO, send, emit
import socket
import select
import os
import time
import threading


app = Flask(__name__)
#app.config['SECRET_KEY'] = secretkey
socketio = SocketIO(app, async_mode='threading')


@app.route('/wsgui')
def index():
  return redirect(url_for('static', filename='wsgui.html'))


@socketio.on('json')
def handle_json(json):
  print('received json: ' + str(json))


@socketio.on('logtoggle')
def toggle_logging(data):
  #print('received json: ' + str(data))
  #emit("info", "starting logging")
  process_act(data)


@socketio.on('clientconn')
def handle_client_connection(json):
  print('received json: ' + str(json))
  emit("info", "web backend connected")    
  

@socketio.on('status change')
def toggle_status_info(data):
  print('server recv: ' + str(data))
  print ("starting", data)
  message = ""
  if str(data) == "Disable":
    message = "continuous checking disabled"
  if str(data) == "Enable":
    message = "continuous checking enabled"
  print ("emitting:", message)
  #emit('sensor status', message)
  emit('info', 'server recv: ' + str(data))


@socketio.on('sensorstatreq')
def send_sensor_status(data):
  print("sensor status requested")
  process_act(data)


@socketio.on('startBg')
def startBg(data):
  print ("startBg")
  open_connection_thr()


@socketio.on('stopBg')
def stopBg(data):
  print ('stopBG called, closing stuff')  
  stop_th()
  emit("info", "BG closed, backend needs restart.")
  #print ("bgclose info sent")




""" --------------------- """
""" bg socket connections """
""" --------------------- """


# based on https://www.oreilly.com/library/view/python-cookbook/0596001673/ch06s03.html 
class TBConnThread(threading.Thread):

  def __init__(self, name='BGConnThread'):
    """ constructor, setting initial variables """
    self._stopevent = threading.Event()
    self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    self.server_address = '/tmp/9Lq7BNBnBycd6nxy.socket'

    threading.Thread.__init__(self, name=name)


  def run(self):
    sockOpen = False
    """ main control loop """
    print ("%s starts" % (self.getName(),))
    print ('connecting to ', self.server_address)
    try:
      self.sock.connect(self.server_address)
      infomsg = "ControlSW connected"
      sockOpen = True    
      #print ("end of try") 
    except Exception as e:
      #emit('info', errmsg + str(e))
      errmsg = "Error connecting to controlSW: "
      infomsg = errmsg + str(e)    
      print (errmsg, e)
    finally:
      socketio.emit('info', infomsg)
      print (infomsg)
    
    inputs = [self.sock]
    count = 0
    buffersize = 65536
    while not self._stopevent.isSet():
      #count += 1
      #print ("loop %d, try select" % (count,))
      infds, outfds, errfds = select.select(inputs, [], [], 30)
      if len(infds) != 0:
        print ("reading socket...")
        data = self.sock.recv(buffersize)
        if len(data) != 0:
          print ('received ', data)
          print ("tbcontrol:", data.decode("utf-8"))
          if data.decode("utf-8").split(":")[0] == "Status":
            socketio.emit('sensor status', data.decode("utf-8"))            
          else:
            socketio.emit('info', data.decode("utf-8"))
        else:
          print ("no data")
      print ("outlen", len(outfds))  

    print ("BGconn loop ended")

    if sockOpen:
      self.sock.close()
    #emit("info", "BG closed, backend needs restart.")


  def join(self, timeout=None):
    """ Stop the thread. """
    self._stopevent.set()
    threading.Thread.join(self, timeout)


  def create_startstr(self, selected_sensors):
    #socketio.emit('info', 'create str')
    startstr = "s"
    #sensors = ["awinda", "ublox", "T265", "B210"]
    sensors = ["awinda", "ublox", "T265", "B210", "refImu"]
    #sensorchars = ["a","u","t","b"]
    sensorchars = ["a","u","t","b", "r"]
    print ("sensorlist", selected_sensors)
    for i in range(0, len(sensorchars)):
      print (sensors[i])
      if sensors[i] in selected_sensors:
        startstr = startstr+sensorchars[i]+str(1)
      else:
        startstr = startstr+sensorchars[i]+str(0)
      
    print ("created startstr", startstr)
    return startstr
    
  
  def process_action(self, data):
    #socketio.emit('info', 'process action')
    if data["action"] == "start":
      startstr = self.create_startstr(data["sensors"])  
      self.nextAction = startstr
    if data["action"] == "stop":  
      self.nextAction = "end"

    if data["action"] == "info":  
      self.nextAction = "info"

    if data["action"] == "opt": #conf
      self.nextAction = data["action"] + ":" + data["opt"]
    try:
      #socketio.emit('info', 'process action try')
      #print ("sending", self.nextAction)
      #socketio.emit('info', self.nextAction)
      #bstr = bytes(self.nextAction, 'utf-8')
      bstr = self.nextAction.encode('utf-8')
      #socketio.emit('info', 'bytestr created')
      self.sock.send(bstr)
      #socketio.emit('info', "sending cmd " + data["action"] + " to bg")
    except Exception as e:
      errmsg = "Error with logging: "
      print (errmsg, e)
      #socketio.emit('info', errmsg)
      socketio.emit('info', errmsg + str(e)) 



tbthread = TBConnThread()
#connthreadOn = False
    
def open_connection_thr():
  tbthread.start()
  #connthreadOn = True


#def openConn():
#  if not connthreadOn:
#    open_connection_thr()
  

def stop_th():
  print ("stop th")
  emit('info', "closing socket ...")
  tbthread.join()


def process_act(data):
  #socketio.emit('info', 'process act')
  return tbthread.process_action(data)



if __name__ == '__main__':
    socketio.run(app)

