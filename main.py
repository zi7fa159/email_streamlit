import requests
from concurrent.futures import ThreadPoolExecutor, as_completed
import streamlit as st

# Function to send a POST request and check the status code
def send_request(session, url, data):
    try:
        response = session.post(url, data=data)
        if response.status_code == 200:
            return True  # Success
        else:
            return False  # Failure
    except Exception as e:
        return False  # Any exception is treated as a failure

# Multithreaded request execution for the first API
def run_multithreaded_requests(num_requests, num_threads, url, data):
    success_count = 0
    fail_count = 0
    
    # Use a session to reuse connection pools (faster requests)
    with requests.Session() as session:
        with ThreadPoolExecutor(max_workers=num_threads) as executor:
            futures = [executor.submit(send_request, session, url, data) for _ in range(num_requests)]

            # Collect results as they are completed
            for future in as_completed(futures):
                result = future.result()
                if result:
                    success_count += 1
                else:
                    fail_count += 1

                # Streamlit updates
                st.session_state.progress += 1
                st.session_state.successes = success_count
                st.session_state.failures = fail_count

# Function to prepare data for the second API
def prepare_data(email):
    return f"email={email}&pass=&type=email&access_token="

# Streamlit UI
st.title("Multithreaded Requests App")
st.write("This app sends multiple concurrent POST requests using multithreading.")

# User input fields
email = st.text_input("Enter your email", "")
num_threads = st.number_input("Number of threads to use", min_value=1, max_value=50, value=5)
counter_limit = st.number_input("Number of requests to send", min_value=1, max_value=1000, value=100)

# URLs for the POST requests
url1 = 'https://70games.net/user-send_code-user_create.htm'
url2 = 'https://prankhotline.com/api/?a=login'

# Button to start the process
if st.button("Start Requests"):
    # Reset session state
    st.session_state.progress = 0
    st.session_state.successes = 0
    st.session_state.failures = 0

    st.write(f"Sending {counter_limit} requests using {num_threads} threads...")

    # Prepare data for the first API request
    data1 = {
        'username': 'hdjdjd',
        'password': email,  # Using email as password for demo
        'inviter': '',
        'email': email,
        'code': '',
    }

    # Run the first multithreaded requests function
    run_multithreaded_requests(counter_limit, num_threads, url1, data1)

    # Prepare data for the second API request
    data2 = prepare_data(email)

    # Run the second multithreaded requests function
    run_multithreaded_requests(counter_limit, num_threads, url2, data2)

    st.success("Requests completed!")

# Progress bar and statistics display
progress_bar = st.progress(st.session_state.progress / (counter_limit * 2))  # Multiply by 2 for two requests
st.write(f"Progress: {st.session_state.progress}/{counter_limit * 2}")
st.write(f"Successes: {st.session_state.successes}")
st.write(f"Failures: {st.session_state.failures}")
