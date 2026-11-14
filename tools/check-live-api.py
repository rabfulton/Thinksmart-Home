#!/usr/bin/env python3
"""Brief Live configuration check. No microphone audio or spoken reply requested."""
import argparse,json,re,time
from pathlib import Path
import websocket
p=argparse.ArgumentParser();p.add_argument('--config',default='private/voice.json');p.add_argument('--backend',default='gpt-6-luna');p.add_argument('--voice',default='marin');args=p.parse_args()
config=json.loads(Path(args.config).read_text());ws=None;started=False;closed=False
safe=lambda x:re.sub(r'[^A-Za-z0-9_.\[\]-]','',str(x))[:120]
try:
 ws=websocket.create_connection('wss://api.openai.com/v1/live/sessions',header={'Authorization':'Bearer '+config['apiKey']},timeout=12)
 ws.send(json.dumps({'type':'session.start','event_id':'thinksmart_probe','session':{'model':'gpt-live-1','store':False,'instructions':'Remain silent until the user speaks.','audio':{'format':{'type':'audio/pcm','rate':24000},'output':{'voice':args.voice}},'delegation':{'type':'responses','responses':{'model':args.backend,'instructions':'Use tools for device data.','tools':[{'type':'function','name':'device_action','description':'Read device status','parameters':{'type':'object','properties':{'action':{'type':'string'}},'required':['action'],'additionalProperties':False},'strict':False}],'tool_choice':'auto','parallel_tool_calls':False,'max_output_tokens':1024}}}}))
 deadline=time.monotonic()+12
 while time.monotonic()<deadline:
  event=json.loads(ws.recv());kind=event.get('type')
  if kind=='error':
   error=event.get('error',{});print('Live configuration rejected:',safe(error.get('code')),safe(error.get('param')));raise SystemExit(1)
  if kind=='session.started':
   started=True;print('Live session started; backend',args.backend,'voice',args.voice)
   ws.send(json.dumps({'type':'session.close','event_id':'thinksmart_probe_close'}))
  if kind=='session.closed':
   closed=True;print('Live finalized; billed seconds:',event.get('usage',{}).get('seconds','unreported'));break
 if not(started and closed):raise RuntimeError('Incomplete session lifecycle')
except Exception as e:
 print('Live check failed:',type(e).__name__,'HTTP',getattr(e,'status_code','n/a'));raise SystemExit(1) from None
finally:
 if ws is not None:ws.close(timeout=2)
