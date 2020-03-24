# MySQL database access
# Takes values from incoming MQTT trigger and inserts them into the DB
# Trigger (new) :  SELECT *, topic() as topic FROM 'myorg/+/status/#'

import sys
#import logging
import pymysql
import json

rds_host = 'my.xxxxxxx.us-east-2.rds.amazonaws.com' 
user_name = 'user'
passwd = 'passwd'
db_name = 'mydb'
port = 3306

# Note how a long string can be split across multiple lines using brackets
# Note how the double quotes can be retained all the way upto the SQL statement
SQL = ('insert into IotData '
       '(OrgId, GroupId, DeviceId, Relays, Temperature, Humidity, HIndex, Light, Pir, Radar) '
       'values ({org},"{gro}","{dev}","{rel}",{tem:.1f},{hum:.1f},{hin:.1f},{lig},{pir},{rad})')

# place holder data object
data = {
    "ORG" : 1,   # TODO: insert the org_id at  the back end from session data
    "GRO" : "G0", 
    "DEV" : "000000", 
    "REL" : "00",
    "TEM" : 0.0, 
    "HUM" : 0.0, 
    "HIN" : 0.0,
    "LIG" : 0,
    "PIR" : 0,
    "RAD" : 0
}

# NOTE: better to put the connection handling at the top of the lambda file, so you can refer to it
# from multiple functions
       
conn = None       
try:
    conn = pymysql.connect(rds_host, user=user_name, passwd=passwd, db=db_name, connect_timeout=8)
    print ("--- Connected to RDS mysql.")
except Exception as e:
    print("---- Could not connect to MySql: ", e)
    #sys.exit()  # TODO: where to exit ?! if not exit, what else to do?

def lambda_handler(event, context):
    if (conn == None):
        print("---- Database connection is invalid")
        return ("DB connection failure")
    try:
        print(json.dumps(event))
        if ('D' not in event):  # todo: process 'S', B' and 'I' packets also
            print("-- Non-data packet")
            return ("Non-data packet")
        topic_fragments = event['topic'].split('/')
        data['DEV'] = topic_fragments[-1] # device id
        data['GRO'] = topic_fragments[-2] # group
        data['REL'] = event['D']['S']  # relay status
        data['TEM'] = event['D']['T']  # temperature
        data['HUM'] = event['D']['H']  # humidity
        data['HIN'] = event['D']['I']  # heat index
        data['LIG'] = event['D']['L']  # light
        data['PIR'] = event['D']['R']  # PIR hit count
        data['RAD'] = event['D']['M']  # radar hit count
        with conn.cursor() as cur:
            #cur.execute('use intof_iot')
            cur.execute(get_sql())
            conn.commit()
            cur.execute("select count(*) from IotData")
            print ("-- Number of rows in table: ", end='')
            for row in cur:
                print(row)
        return ("DB operation succeeded")
    except Exception as e:
        print ('---- Exception ! ', e)
        return ('DB opertion failed')
        
def get_sql():
    # .format does not destroy the original string
    sqlstr = SQL.format (org=data['ORG'], gro=data['GRO'], dev=data['DEV'], rel=data['REL'],
                        tem=data['TEM'], hum=data['HUM'], hin=data['HIN'], lig=data['LIG'], 
                        pir=data['PIR'], rad=data['RAD'])
    print (sqlstr)
    return(sqlstr)
         
#print (lambda_handler('event', 'context'))