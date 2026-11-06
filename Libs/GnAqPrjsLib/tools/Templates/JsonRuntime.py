# JsonRuntime.py, Version: 1.02
import copy,datetime,json,math,re

def _check_type(Type,Value):
  if Type in ('TGnJsonNull','TJsonNull'):return Value is None
  if Type in ('TGnJsonValue','TJsonValue'):return Value is None or type(Value) in (str,bool) or type(Value) in (int,float) and math.isfinite(Value) or isinstance(Value,list) and all(_check_type(Type,X) for X in Value) or isinstance(Value,dict) and all(isinstance(K,str) and _check_type(Type,X) for K,X in Value.items())
  if Type=='TGNString':return isinstance(Value,str)
  if Type=='bool':return type(Value) is bool
  if Type=='double':return type(Value) in (int,float) and math.isfinite(Value)
  if Type.startswith('uint'):
    return type(Value) is int and 0<=Value<(1<<int(Type[4:Type.index('_')]))
  if Type=='TGnChannelId':return isinstance(Value,str) or type(Value) is int and 0<=Value<1<<64
  if Type.startswith('VariantOf('):return any(_check_type(X,Value) for X in _split_rules(Type[10:-1]))
  if Type.startswith('ArrayOf('):return isinstance(Value,(list,tuple)) and all(_check_type(Type[8:-1],X) for X in Value)
  if Type.startswith('MapOf('):return isinstance(Value,dict) and all(isinstance(K,str) and _check_type(Type[6:-1],X) for K,X in Value.items())
  return ValidateType(Type,Value)

def _split_rules(Text):
  Start=Depth=0;Parts=[]
  for Index,Char in enumerate(Text):
    if Char=='(':Depth+=1
    elif Char==')':Depth-=1
    elif Char==',' and Depth==0:Parts.append(Text[Start:Index]);Start=Index+1
  Parts.append(Text[Start:])
  return Parts

def _rule(Rule,Value,Row):
  if Rule.startswith('RequiredIf('):Rule=Rule.split(',',1)[1][:-1]
  if Rule.startswith(('RequiredUnless(', 'RequiredWhen(')):Rule=Rule.split(',',2)[2][:-1]
  if Rule.startswith('Each('):return all(_rule(Rule[5:-1],X,Row) for X in (Value.values() if isinstance(Value,dict) else Value))
  if Rule.startswith('All('):return all(_rule(X,Value,Row) for X in _split_rules(Rule[4:-1]))
  if Rule.startswith('ParentTextUnless('):
    Trigger,Expected,Parent,Text=Rule[17:-1].split(',')
    return Value.get(Trigger)==Expected or Parent not in Row or Row[Parent]==Text
  if Rule.startswith('RangeIf('):
    Flag,Low,High=Rule[8:-1].split(',');return Row.get(Flag) is not True or float(Low)<=Value<=float(High)
  if Rule.startswith('TextIf('):
    Flag,Text=Rule[7:-1].split(',');return Row.get(Flag) is not True or Value==Text
  if Rule.startswith('Choice('):return Value in Rule[7:-1].split(',')
  if Rule.startswith('Positive('):return 0<Value<=float(Rule[9:-1])
  if Rule.startswith('TextLength('):
    Low,High=map(int,Rule[11:-1].split(','));return Low<=len(Value.encode('utf-8'))<=High
  if Rule.startswith('EmptyOrLength('):
    Low,High=map(int,Rule[14:-1].split(','));return not Value or Low<=len(Value.encode('utf-8'))<=High
  if Rule.startswith('ServiceIds('):
    return _rule('WireId',Value,Row) and int(Value[-1]) in map(int,Rule[11:-1].split(','))
  if Rule=='EmptyOrUuid':return not Value or re.fullmatch(r'[0-9a-fA-F]{8}-(?:[0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12}',Value) is not None
  if Rule in ('TimeOfDay','Duration'):
    return re.fullmatch(r'\d{2}:\d{2}',Value) is not None and int(Value[3:])<60 and (Rule=='Duration' or int(Value[:2])<24)
  if Rule=='MeasurementName':return re.fullmatch(r'[A-Za-z0-9_]{1,64}',Value) is not None and Value!='Kind'
  if Rule=='SensorId':return re.fullmatch(r'28-[0-9a-fA-F]{12}',Value) is not None
  if Rule=='Ipv4':return len(Value.split('.'))==4 and all(X.isascii() and X.isdigit() and 0<=int(X)<=255 for X in Value.split('.'))
  if Rule.startswith('WireIdIf('):return Row.get(Rule[9:-1]) is not True or _rule('WireId',Value,Row)
  if Rule.startswith('ItemsHaveIf('):
    Trigger,Items,Key=Rule[12:-1].split(',')
    return Row.get(Trigger) is not True or isinstance(Value,dict) and all(isinstance(X,dict) and Key in X for X in Value.get(Items,[]))
  if Rule=='PositiveDuration':return _rule('Duration',Value,Row) and Value!='00:00'
  if Rule.startswith('DurationBefore('):return _rule('PositiveDuration',Value,Row) and isinstance(Row.get(Rule[15:-1]),str) and Value<Row[Rule[15:-1]]
  if Rule=='FitsFloat':return abs(Value)<=3.4028234663852886e38
  if Rule in ('ChannelAddress','ServiceAddress','OutputAddress'):
    Match=re.fullmatch(r'((?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2})(?:@([1-4]))?(?:/([0-9]+))?',Value)
    return Match is not None and Match[1]!='00:00:00:00:00:00' and (Match[3] is None or int(Match[3])<=4294967295) and (Rule!='ServiceAddress' or Match[2] is not None and Match[3] is None)
  if Rule in ('HttpUrl','WebSocketUrl'):
    Scheme=r'https?' if Rule=='HttpUrl' else r'wss?'
    return re.fullmatch(Scheme+r'://[^/\s]+(?:/[^\s]*)?',Value) is not None
  if Rule=='TemperatureProfileIfActive':
    if Value.get('Active') is not True:return True
    Keys=('Setpoint','FloorW','MainMaxW','CapacityWhPerK','LossWPerK','Kp','TiSec','WindowSec','DelaySec')
    return Row.get('Meas')=='Temperature' and all(type(Value.get(K)) in (int,float) for K in Keys) and all(Value[K]>0 for K in ('CapacityWhPerK','Kp','TiSec','WindowSec')) and all(Value[K]>=0 for K in ('FloorW','MainMaxW','LossWPerK'))
  if Rule=='Positive':return Value>0
  if Rule in ('Any','Finite'):return True
  if Rule=='NonEmpty':return bool(Value)
  if Rule=='Hex16':return re.fullmatch(r'[0-9a-fA-F]{16}',Value) is not None
  if Rule in ('WireId','SourceId'):
    return re.fullmatch(r'(?:[0-9A-F]{2}:){5}[0-9A-F]{2}@'+('3' if Rule=='SourceId' else '[1-4]'),Value) is not None and Value[:17]!='00:00:00:00:00:00'
  if Rule=='Revision':
    if re.fullmatch(r'\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z',Value,flags=re.ASCII) is None:return False
    try:datetime.datetime.strptime(Value,'%Y-%m-%dT%H:%M:%S.%fZ')
    except ValueError:return False
    return True
  if Rule=='TrueValue':return Value is True
  Kind,Args=Rule.split('(',1);Args=Args[:-1].split(',')
  if Kind=='Range':return float(Args[0])<=Value<=float(Args[1])
  if Kind=='Count':return int(Args[0])<=len(Value)<=int(Args[1])
  if Kind=='Utf8Bytes':return len(Value.encode('utf-8'))<=int(Args[0])
  if Kind=='Below':return Args[0] not in Row or type(Row.get(Args[0])) in (int,float) and Value<Row[Args[0]]
  raise ValueError('Unknown schema rule '+Rule)

def _ValidateType(Name,Value,Partial=False,Only=None):
  Schema=TYPES[Name]
  if Schema.get('Alias'):return _check_type(Schema['Alias'],Value) and _rule(Schema['Rule'],Value,Value)
  if 'Enum' in Schema:return type(Value) is int and any(X['Value']==Value and not X['Name'].endswith('_MAX') for X in Schema['Enum'])
  if Schema.get('ValueType'):return _check_type(Schema['ValueType'],Value) and _rule(Schema['Rule'],Value,Value)
  if Schema.get('Shape')=='Tuple':
    Fields=Schema['Fields'];Required=sum(F['Presence'].startswith('Required') for F in Fields)
    return isinstance(Value,(list,tuple)) and Required<=len(Value)<=len(Fields) and all((X is None and (ACCEPT_NULL or F['Presence'].endswith('Nullable'))) or _check_type(F['Type'],X) and _rule(F['Rule'],X,{A['Name']:B for A,B in zip(Fields,Value)}) for F,X in zip(Fields,Value))
  if not isinstance(Value,dict):return False
  for Field in Schema['Fields']:
    if Only is not None and Field['Name'] not in Only:continue
    Key=Field['Name'];Presence=Field.get('Presence','Required')
    if Key not in Value:
      Rule=Field.get('Rule','')
      Conditional=False
      if Rule.startswith(('RequiredUnless(', 'RequiredWhen(')):
        Kind=Rule.split('(',1)[0];Trigger,Expected=Rule.split('(',1)[1].split(',',2)[:2]
        Conditional=(Value.get(Trigger)!=Expected) if Kind=='RequiredUnless' else (Value.get(Trigger)==Expected)
      if Conditional or not Partial and Presence.startswith('Required') or Rule.startswith('RequiredIf(') and Value.get(Rule.split('(',1)[1].split(',',1)[0]) is True:return False
      continue
    X=Value[Key]
    if X is None and (ACCEPT_NULL or Presence.endswith('Nullable')) and Field['Kind']!='Const':continue
    if not _check_type(Field['Type'],X):return False
    if Field['Kind']=='Const':
      if X!=Field['Value']:return False
    elif not _rule(Field['Rule'],X,Value):return False
  return True

def ValidateType(Name,Value,Partial=False):
  try:return _ValidateType(Name,Value,Partial)
  except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError):return False

def ValidateMessage(Packet):
  if not isinstance(Packet,dict) or type(Packet.get('Type')) not in (str,int):return False
  Name=MESSAGES.get(Packet['Type'])
  return Name is not None and ValidateType(Name,Packet)

def ValidateMembers(Name,Value,Members):
  try:return _ValidateType(Name,Value,Only=Members)
  except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError):return False

def ReadJson(Name,Text):
  Value=json.loads(Text)
  if not ValidateType(Name,Value):raise ValueError('Invalid JSON contract: '+Name)
  return Value

def WriteJson(Name,Value,**Options):
  if not ValidateType(Name,Value):raise ValueError('Invalid JSON contract: '+Name)
  return json.dumps(Value,ensure_ascii=False,allow_nan=False,**Options)

def ReadMessage(Packet):
  if not ValidateMessage(Packet):raise ValueError('Invalid UDP message')
  return copy.deepcopy(Packet)

def WriteMessage(Packet):
  if not ValidateMessage(Packet):raise ValueError('Invalid UDP message')
  return json.dumps(Packet,ensure_ascii=False,separators=(',',':'),allow_nan=False)

def DispatchMessage(Packet,Receiver):
  if not ValidateMessage(Packet):return False
  Tag=Packet['Type']
  if type(Tag) is int:
    Tag=next((Item['Name'] for Item in TYPES.get('TMsgType',{}).get('Enum',[]) if Item['Value']==Tag),None)
  Handler=getattr(Receiver,'RxMsg_'+str(Tag),None)
  if Handler is None:return False
  Handler(Packet)
  return True
