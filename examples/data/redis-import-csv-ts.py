import sys
import redis
import getopt
import fileinput
import csv
from decimal import Decimal

r = redis.Redis(decode_responses=True)
ts = r.ts()

r.ping()

def main(argv):
  inputfile = ""
  keyfield = ""

  try:
    opts, args = getopt.getopt(argv,"i:k:",["ifile=","keyfield="])
  except getopt.GetoptError:
    print('redis-import-csv-ts.py -i <inputfile> -k <ts key>')
    sys.exit(2)
  for opt, arg in opts:
    if opt == '-h':
       usage()
       sys.exit()
    elif opt in ("-i", "--ifile"):
       inputfile = arg
    elif opt in ("-k", "--keyfield"):
       keyfield = arg

  if not inputfile:
    print("ERROR: -i is missing")
    usage()
    sys.exit(2)

  if not keyfield:
    print("ERROR: -k is missing")
    usage()
    sys.exit(2)

  convert_file(inputfile, keyfield)

def usage():
  print('redis-import-csv-ts.py -i <inputfile> -k <ts-key>')

headers = None

def convert_file(input, key):
  count = 0
  print(f"Creating new key {key}")
  ts.create(key)

  for line in fileinput.input(input):
    count = count + 1
    values = parse(line)
    if count == 1:
      global headers
      headers = values
    else:
      timestamp = int(values[0])
      value = float(values[1])
      ts.add(key, timestamp, value)



def parse(line):
  reader = csv.reader([line], skipinitialspace=True, delimiter=',')
  for r in reader:
    return r
  return None

if __name__ == "__main__":
  main(sys.argv[1:])
