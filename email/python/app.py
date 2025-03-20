import uvicorn
from fastapi import FastAPI, Depends, HTTPException, status, Request, Body
from fastapi.responses import StreamingResponse, Response, JSONResponse, HTMLResponse, RedirectResponse
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from fastapi.templating import Jinja2Templates
from pydantic import BaseModel
import sys, os, json, time
import redis

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from _email import *

app = FastAPI()

class EmailRequest(BaseModel):
    email: str


# 连接到 Redis 服务器
redis_template_verification_code = redis.Redis(host='localhost', port=6379, db=1)


@app.api_route("/email", methods=["POST"])
async def email(request: EmailRequest):
    
    
    try:
        print(request.email)
        verification_code = generate_verification_code()
        redis_template_verification_code.set(request.email, verification_code, ex=300)
        aa = send_verification_email(request.email, verification_code)
        if aa:
            data = {
                "isok": True
            }
        else:
            data = {
                "isok": False
            }
    except Exception as e:
        print(f"{e}")
        data = {
            "isok": False
        }
    finally:
        return JSONResponse(content=json.dumps(data, separators={'', ':'}))
    


if __name__ == "__main__":
    # 使用 uvicorn 的编程接口启动应用
    uvicorn.run(app, host="127.0.01", port=8000)