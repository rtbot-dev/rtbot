import rtbot.libs.core.wrappers.python.rtbotapi as api
import json
import nanoid

from typing import Optional
from json import JSONEncoder

class Program(object):
    def __init__(self, 
                 title: Optional[str] = None,
                 description: Optional[str] = None,
                 apiVersion: Optional[str] = "v1",
                 date: Optional[str] = None,
                 author: Optional[str] = None,
                 licencse: Optional[str] = None):
        self.title = title
        self.description = description
        self.apiVersion = apiVersion
        self.date = date
        self.author = author
        self.license = licencse
        self.operators = []
        self.connections = []
        self.programId = nanoid.generate('0123456789abcdef', 3)

    def addOperator(self, arg):
        pass

    def removeOperator(self, arg):
        pass

    def addConnection(self, arg):
        pass

    def removeConnection(self, arg):
        pass


class ProgramEncoder(JSONEncoder):
    def default(self, o):
        return o.__dict__

class Connection(object):
    def __init__(self, fromOp: str, toOp: str, fromPort: str, toPort: str):
        self.fromOp = fromOp
        self.toOp = toOp
        self.fromPort = fromPort
        self.toPort = toPort

class ConnectionEncoder(JSONEncoder):
    def default(self, o):
        return { "from": o.fromOp, "to": o.toOp, "fromPort": o.fromPort, "toPort": o.toPort }
