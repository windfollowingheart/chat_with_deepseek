import smtplib, random, time
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart

# 生成 6 位随机验证码
def generate_verification_code():
    return ''.join([str(random.randint(0, 9)) for _ in range(6)])
def get_email_server():
    # 发件人 QQ 邮箱和授权码
    sender_email = "2744244824@qq.com"
    authorization_code = "lezvcfowsqekddih"
    # 连接到 QQ 邮箱的 SMTP 服务器
    server = smtplib.SMTP_SSL("smtp.qq.com", 465)
    # 登录发件人邮箱
    server.login(sender_email, authorization_code)
    return server

# 发送验证码邮件
def send_verification_email(receiver_email, verification_code):
    sender_email = "2744244824@qq.com"
    server = get_email_server()
    # verification_code = generate_verification_code()
    # 构建邮件内容
    message = MIMEMultipart()
    # message["From"] = sender_email
    message["From"] = sender_email
    message["To"] = receiver_email
    message["Subject"] = "DeepSeekAnyWhere 验证码 - 请查收"

    body = f"您的验证码是：{verification_code}，请在5分钟内完成验证。"
    message.attach(MIMEText(body, "plain"))

    try:
        # 发送邮件
        text = message.as_string()
        server.sendmail(sender_email, receiver_email, text)
        print("验证码邮件发送成功！")
        return verification_code
    except Exception as e:
        print(f"邮件发送失败：{e}")
        return None
    finally:
        # 关闭 SMTP 连接
        server.quit()

# 示例使用
if __name__ == "__main__":
    receiver_email = "wangqiteng@hust.edu.cn"
    server = get_email_server()
    # verification_code = generate_verification_code()
    for i in range(2):
        start_time = time.time()
        # verification_code = generate_verification_code()
        send_verification_email(receiver_email)
        # time.sleep(1)
        print(f"第{i+1}次发送邮件耗时：{time.time() - start_time}秒")
    