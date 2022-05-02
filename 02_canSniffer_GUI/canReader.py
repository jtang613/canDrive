from PyQt5.QtCore import QThread, pyqtSignal
import can
import time


class canReaderThread(QThread):
    receivedPacketSignal = pyqtSignal(str, float)
    buf = bytearray()

    def __init__(self, bus=None):
        super(canReaderThread, self).__init__()
        self.bus = bus
        self.isRunning = False

    def stop(self):
        self.isRunning = False

    def run(self):
        self.isRunning = True
        while self.isRunning:
            try:
                msg = self.bus.recv()
                data = '{:X},{:02X},{:02X},{}\n'.format(msg.arbitration_id, msg.is_extended_id, msg.dlc, ''.join('{:02X}'.format(x) for x in msg.data)).encode('utf8')
                try:
                    decodedData = data.decode("utf-8")
                    self.receivedPacketSignal.emit(decodedData, time.time())
                except UnicodeDecodeError as e:
                    print(e)
            except can.CanError as e:
                print(e)
                pass
                # There is no new data from can port
            except TypeError as e:
                print("Serial disconnected")
                print(e)
                self.isRunning = False
        self.msleep(10)
