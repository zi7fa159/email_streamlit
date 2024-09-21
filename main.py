import requests
from concurrent.futures import ThreadPoolExecutor, as_completed
import streamlit as st

# Function to send a POST request and check the status code
def send_request(session, url, data):
    try:
        response = session.post(url, data=data)
        return response.status_code == 200  # True for success, False for failure
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
    
    with requests.Session() as session:
        with ThreadPoolExecutor(max_workers=num_threads) as executor:
            futures = {executor.submit(send_request, session, url, data): i for i in range(num_requests)}

            # Collect results as they are completed
            for i, future in enumerate(as_completed(futures)):
                result = future.result()
                if result:
                    success_count += 1
                else:
                    fail_count += 1

                # Update progress and statistics in real-time
                progress_bar.progress((i + 1) / num_requests)
                progress_placeholder.text(f"Progress: {i + 1}/{num_requests}")
                success_placeholder.text(f"Successes: {success_count}")
                fail_placeholder.text(f"Failures: {fail_count}")

                # Check if stop is requested
                if st.session_state.stop:
                    break

    return success_count, fail_count

# Streamlit UI
st.title("Multithreaded Requests App")
st.write("This app sends multiple concurrent POST requests using multithreading.")

# User input fields
email = st.text_input("Enter your email", value=st.session_state.get("email", ""))
num_threads = st.number_input("Number of threads", min_value=1, max_value=50, value=5)
counter_limit = st.number_input("Number of requests to send", min_value=1, max_value=1000, value=100)

# URL and data for the POST request
url = 'https://70games.net/user-send_code-user_create.htm'
data = {
    'username': 'hdjdjd',
    'password': email,  # Using email as password for demo purposes
    'inviter': '',
    'email': email,
    'code': '',
}

# Initialize session state variables
if 'stop' not in st.session_state:
    st.session_state.stop = False
if 'in_progress' not in st.session_state:
    st.session_state.in_progress = False

# Button to start/stop requests
if st.session_state.in_progress:
    if st.button("Stop Requests"):
        st.session_state.stop = True
else:
    if st.button("Start Requests"):
        st.session_state.stop = False
        st.session_state.in_progress = True
        st.session_state.email = email  # Store the email in session state
        st.write(f"Sending {counter_limit} requests using {num_threads} threads...")

        # Run the multithreaded requests function
        run_multithreaded_requests(counter_limit, num_threads, url, data)

        st.session_state.in_progress = False
        st.success("Requests completed!")

# Reset button to clear email input
if st.button("Reset"):
    st.session_state.email = ""
    st.session_state.stop = False
    st.session_state.in_progress = False
    st.session_state.progress = 0  # Reset progress
    st.session_state.successes = 0  # Reset successes
    st.session_state.failures = 0  # Reset failures

# Progress bar and statistics display
st.write(f"Successes: {st.session_state.get('successes', 0)}")
st.write(f"Failures: {st.session_state.get('failures', 0)}")
