import requests
from concurrent.futures import ThreadPoolExecutor, as_completed
import streamlit as st

# Function to send a POST request and check the status code
def send_request(session, url, data):
    try:
        response = session.post(url, data=data)
        return response.status_code == 200  # Return True if success (200)
    except Exception:
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
            futures = {executor.submit(send_request, session, url, data): i for i in range(num_requests)}

            for i, future in enumerate(as_completed(futures), 1):
                if not st.session_state.in_progress:  # Check if still in progress
                    break  # Stop processing if user requested to stop

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

    # Reset progress if finished or stopped
    if st.session_state.in_progress:
        st.session_state.in_progress = False
        st.success("Requests completed!")
    else:
        st.warning("Requests stopped!")

# Streamlit UI
st.title("Multithreaded Requests App")
st.write("This app sends multiple concurrent POST requests using multithreading.")

# User input fields
email = st.text_input("Enter your email", "")
num_threads = st.number_input("Number of threads", min_value=1, max_value=50, value=5)
counter_limit = st.number_input("Number of requests to send", min_value=1, max_value=1000, value=100)

# Initialize session state for controlling progress
if 'in_progress' not in st.session_state:
    st.session_state.in_progress = False

# URL and data for the POST request
url = 'https://70games.net/user-send_code-user_create.htm'
data = {
    'username': 'hdjdjd',
    'password': email,  # Using email as password for demo purposes
    'inviter': '',
    'email': email,
    'code': '',
}

# Button to start/stop the process
if st.button("Start Requests" if not st.session_state.in_progress else "Stop Requests"):
    if not st.session_state.in_progress:
        if email:
            st.session_state.in_progress = True  # Mark as in progress
            st.write(f"Sending {counter_limit} requests using {num_threads} threads...")
            run_multithreaded_requests(counter_limit, num_threads, url, data)
        else:
            st.error("Please enter a valid email.")
    else:
        st.session_state.in_progress = False  # Mark as not in progress

# Progress bar and statistics display
if st.session_state.in_progress:
    st.write("Requests are in progress...")
else:
    st.write("Requests are not in progress.")
