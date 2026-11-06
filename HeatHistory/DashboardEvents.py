# DashboardEvents.py, Version: 1.01
"""Receive configuration SSE and wake the recording event loop."""
import http.client
from Messages import ReadJson
import queue
import socket
import threading
import urllib.parse

class DashboardEvents:
  """Receive configuration SSE in a worker; wake the main event loop via a socket."""
  def __init__(self,url):
    self.url=urllib.parse.urlsplit(url)
    self.reader,self.writer=socket.socketpair()
    self.reader.setblocking(False)
    self.writer.setblocking(False)
    self.messages=queue.SimpleQueue()
    self.stopped=threading.Event()
    self.connection=None
    self.worker=threading.Thread(target=self.Receive,name="DashboardEvents",daemon=True)

  def Start(self):
    self.worker.start()

  def Publish(self,message):
    self.messages.put(message)
    try:self.writer.send(b'!')
    except (BlockingIOError,OSError):pass

  def Receive(self):
    while not self.stopped.is_set():
      response=None
      try:
        connection_type=http.client.HTTPSConnection if self.url.scheme=='https' else http.client.HTTPConnection
        self.connection=connection_type(self.url.hostname,self.url.port,timeout=30)
        self.connection.request('GET',self.url.path.rstrip('/')+'/api/recording-events',headers={'Accept':'text/event-stream'})
        response=self.connection.getresponse()
        if response.status!=200 or not response.getheader('Content-Type','').startswith('text/event-stream'):
          raise OSError('Dashboard configuration event stream unavailable')
        event='';data=[];length=0
        while not self.stopped.is_set():
          raw=response.fp.readline(1048577)
          if not raw:raise OSError('Dashboard event connection closed')
          length+=len(raw)
          if length>1048576:raise ValueError('Dashboard event exceeds size limit')
          line=raw.decode('utf-8').rstrip('\r\n')
          if not line:
            if event=='configuration' and data:
              message=ReadJson('TSseRecordingEvent','\n'.join(data))
              self.Publish(message)
            event='';data=[];length=0
          elif line.startswith('event:'):event=line[6:].strip()
          elif line.startswith('data:'):data.append(line[5:].lstrip(' '))
      except (OSError,ValueError,http.client.HTTPException) as error:
        if not self.stopped.is_set():self.Publish({'ConnectionError':str(error)})
      finally:
        if response:response.close()
        if self.connection:self.connection.close()
      # Retry only after loss; no periodic configuration requests while connected.
      self.stopped.wait(2)

  def Read(self):
    while True:
      try:
        if not self.reader.recv(4096):break
      except BlockingIOError:break
    while True:
      try:yield self.messages.get_nowait()
      except queue.Empty:return

  def Close(self):
    self.stopped.set()
    if self.connection and self.connection.sock:
      try:self.connection.sock.shutdown(socket.SHUT_RDWR)
      except OSError:pass
    if self.worker.is_alive():self.worker.join(timeout=2)
    self.reader.close();self.writer.close()


