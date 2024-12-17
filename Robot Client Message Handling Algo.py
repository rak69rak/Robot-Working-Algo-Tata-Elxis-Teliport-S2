# Defining the necessary variables
queue_new = []  # Queue for new messages
queue_failed = []  # Queue for failed messages
topic_rate = 10  # rate of the topic (in Hz)
delay_flag = 1  # delay flag (1 for immediate, 0 for delayed)
elapsed_time = 0.1  # time elapsed since the last message (in seconds)

# Function to simulate adding a message to a queue
def push(queue, message_topic_tuple):
    queue.append(message_topic_tuple)

# Function to check if a topic is in the failed queue
def contains(queue, topic):
    return any(item[1] == topic for item in queue)

# Algorithm 1: Local Robot OS Message Handling
def handle_message(message, topic, elapsed_time):
    # If elapsed time is less than the inverse of the rate, ignore the message
    if elapsed_time < 1 / topic_rate:
        return
    
    if delay_flag == 1:
        # Push message to the new queue (immediate processing)
        push(queue_new, (message, topic))
    elif delay_flag == 0:
        # If delay flag is 0, check if the topic is already in the failed queue
        if contains(queue_failed, topic):
            # Update the message for the given topic in the failed queue
            for i, (msg, t) in enumerate(queue_failed):
                if t == topic:
                    queue_failed[i] = (message, topic)
        else:
            # Push the message to the failed queue if it's not already there
            push(queue_failed, (message, topic))
