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

# Multithreaded request execution with real-time progress update
def run_multithreaded_requests(num_requests, num_threads, url, data):
    success_count = 0
    fail_count = 0
    
    # Progress bar and placeholders for success/fail counts
    progress_bar = st.progress(0)
    progress_placeholder = st.empty()
    success_placeholder = st.empty()
    fail_placeholder = st.empty()
    
    # Use a session to reuse connection pools (faster requests)
    with requests.Session() as session:
        with ThreadPoolExecutor(max_workers=num_threads) as executor:
            futures = [executor.submit(send_request, session, url, data) for _ in range(num_requests)]

            # Collect results as they are completed
            for i, future in enumerate(as_completed(futures), 1):
                result = future.result()
                if result:
                    success_count += 1
                else:
                    fail_count += 1

                # Update progress and statistics in real-time
                progress_bar.progress(i / num_requests)
                progress_placeholder.text(f"Progress: {i}/{num_requests}")
                success_placeholder.text(f"Successes: {success_count}")
                fail_placeholder.text(f"Failures: {fail_count}")

# Streamlit UI
st.title("Multithreaded Requests App")
st.write("This app sends multiple concurrent POST requests using multithreading.")

# User input fields
email = st.text_input("Enter your email", "")
num_threads = st.number_input("Number of threads", min_value=1, max_value=9999999, value=50)
counter_limit = st.number_input("Number of requests to send", min_value=1, max_value=999999, value=10)

# URL and data for the POST request
url = 'https://70games.net/user-send_code-user_create.htm'
data = {
    'username': 'hdjdjd',
    'password': email,  # Using email as password for demo purposes
    'inviter': '',
    'email': email,
    'code': '',
}

# Button to start the process
if st.button("Start Requests"):
    if email:
        st.write(f"Sending {counter_limit} requests using {num_threads} threads...")

        # Run the multithreaded requests function
        run_multithreaded_requests(counter_limit, num_threads, url, data)

        st.success("Requests completed!")
    else:
        st.error("Please enter a valid email.")
