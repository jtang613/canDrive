from PyQt5.QtCore import QThread, pyqtSignal
import can
import queue


class canWriterThread(QThread):
    packetSentSignal = pyqtSignal()
    writerQ = queue.Queue()
    tempQ = queue.Queue()
    repeatedWriteDelay = 0
    normalWriteDelay = 0

    def __init__(self, bus=None):
        super(canWriterThread, self).__init__()
        self.bus = bus
        self.isRunning = False

    def clearQueues(self):
        self.writerQ.queue.clear()
        self.tempQ.queue.clear()

    def stop(self):
        self.isRunning = False
        self.clearQueues()

    def write(self, packet):
        self.writerQ.put(packet)

    def setRepeatedWriteDelay(self, delay):
        self.repeatedWriteDelay = delay
        with self.tempQ.mutex:
            self.tempQ.queue.clear()

    def setNormalWriteDelay(self, delay):
        self.normalWriteDelay = delay

    def run(self):
        self.isRunning = True
        while self.isRunning:
            if not self.writerQ.empty():
                element = self.writerQ.get()
                if isinstance(element, list):
                    for it in element:
                        it = it[1:-1].split(',')
                        data = list(bytearray.fromhex(it[3]))
                        msg = can.Message(arbitration_id=int(it[0],16), is_extended_id=bool(it[1]!='00'), dlc=len(data), data=data)
                        try:
                            num = self.bus.send(msg, 1)
                        except can.CanError as e:
                            print(e)
                else:
                    it = element[1:-1].split(',')
                    data = list(bytearray.fromhex(it[3]))
                    msg = can.Message(arbitration_id=int(it[0],16), is_extended_id=bool(it[1]!='00'), dlc=len(data), data=data)
                    try:
                        num = self.bus.send(msg, 1)
                    except can.CanError as e:
                        print(e)

                if self.normalWriteDelay != 0:
                    self.msleep(self.normalWriteDelay)
                    self.normalWriteDelay = 0

                if self.repeatedWriteDelay != 0:
                    self.tempQ.put(element)

                self.packetSentSignal.emit()
            else:
                if self.repeatedWriteDelay != 0 and not self.tempQ.empty():
                    self.msleep(self.repeatedWriteDelay)
                    while not self.tempQ.empty():
                        self.writerQ.put(self.tempQ.get())
                else:
                    self.msleep(1)
