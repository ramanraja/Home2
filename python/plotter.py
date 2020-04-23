# Read IoT database and plot any one variable
 
from config import *
import sys
import pymysql
import json
import numpy as np
import matplotlib.pyplot as plt 
import matplotlib.dates as mdates

# Note how the double quotes can be retained all the way upto the SQL statement
SQL1 = ('select SlNo, GroupId, DeviceId, Timestamp, {param} from IotData '
        'where GroupId = "{grp}" and DeviceId ="{dev}"  and SlNo >= ({total}-{lim})')   
        # order by SlNo DESC will plot it in reverse chronology !
#print (SQL1)
#print ('\n')

counter_sql_str = 'select count(*) from IotData'
#-------------------------------------------------------
params = ['Temperature', 'Humidity', 'Light']
groups = ['G1', 'G2']
dev_ids = [['5CCF7F2CC5AA', '2CF432173BC1', '2CF432173422'], ['98F4ABDCA438', '98F4ABDCD4FA']]

MAX_ROWS = 10000  # to guard agains too large data sets
p=0
g=0
d=0
r=1000

if  len(sys.argv) > 1:
    p = int(sys.argv[1])
if  len(sys.argv) > 2:
    g = int(sys.argv[2])
if  len(sys.argv) > 3:
    d = int(sys.argv[3])
if  len(sys.argv) > 4:
    r = int(sys.argv[4])   
            
PARAMETER =  params[p]       
GROUP = groups[g]
DEVICE_ID = dev_ids[g][d]
LIMIT_ROWS = r
if LIMIT_ROWS > MAX_ROWS:
    print ('Max data set limited to ', MAX_ROWS)
    LIMIT_ROWS = MAX_ROWS

#-------------------------------------------------------     
print ('Usage: python plotx.py [param-0,1,2] [group 0,1] [device {0,1,2}{0,1}] [nrows]')
try:
    conn = pymysql.connect(rds_host, user=user_name, passwd=passwd, db=db_name, connect_timeout=8)
except Exception as e:
    print("ERROR:Could not connect to MySql: ", e)
    sys.exit()  # todo: what to do in a lambda ?
print ("Connected to RDS MySql.")
#-------------------------------------------------------

# globals
num_rows = 0
counter=0
x = []
y = []
def lambda_handler(event, context):
    global counter
    sql_str = SQL1.format (param=PARAMETER, grp=GROUP, dev=DEVICE_ID, total=num_rows, lim=LIMIT_ROWS)
    print (sql_str)
    print ('\n')    
    try:
        with conn.cursor() as cur:
            #cur.execute('use intof_iot')
            cur.execute(sql_str)
            #conn.commit()
            for row in cur:
                #print(row)
                counter += 1
                x.append(row[-2])  # timestamp is the last but one entry
                y.append(row[-1])  # the required param is the last entry
        return ({"result":"success", "rows":counter})
    except Exception as e:
        print ('Exception ! ', e)
        return ({"result":"failed", "rows":0})
#-------------------------------------------------------        
def get_num_rows ():
    global num_rows
    print ('getting row count..')
    try:
        with conn.cursor() as cur:
            cur.execute(counter_sql_str)
            row = cur.fetchone()
            #print (type(row))
            #print(row)   
            num_rows = row[0] 
            print("Number of rows in DB: ", num_rows)        
    except Exception as e:
        print ('Exception ! ', e)
#-------------------------------------------------------
        
if (__name__ == '__main__'):
    get_num_rows();
    if num_rows == 0:
        print ('--- Record count failure ---')
        sys.exit(0)    
    result = lambda_handler("evnt", "context")
    print (result)
    if result['result'] != 'success' :
        print ('--- Data failure ---')
        sys.exit(0)
    if result['rows'] == 0:
        print ('--- Empty data set ---');
        sys.exit(0)    
    print (x[-1].strftime("%m/%d/%Y, %H:%M:%S tz=%Z;  %c"))
    title = '-'.join ((PARAMETER,GROUP,DEVICE_ID))
    print (title)
    #x = range(counter)
    plt.plot(x,y)
    axes = plt.gca()
    formatter = mdates.DateFormatter("%a")
    axes.xaxis.set_major_formatter(formatter)
    locator = mdates.DayLocator()
    axes.xaxis.set_major_locator(locator)
    plt.title(title)
    plt.xticks(rotation=90)
    plt.margins(0.2)
    plt.subplots_adjust(bottom=0.15)
    plt.show()