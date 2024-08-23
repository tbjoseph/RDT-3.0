## Project Overview
This project involves developing a robust, high-performance transport layer service over UDP capable of maintaining high transfer rates even under conditions of packet loss. The service includes functionalities such as connection establishment, data transmission, and connection termination, leveraging customized packet handling and error recovery mechanisms to operate efficiently over lossy networks. 
This transport layer service leverages UDP to provide a reliable, high-performance solution for data transmission over lossy networks, incorporating sophisticated error handling and flow control strategies to deliver superior performance and reliability.

## Key Features
* Reliable Data Transfer: Implements a custom transport layer over UDP, ensuring reliable data transfer despite the unreliable nature of the underlying protocol.
* Connection Handshake and Termination: Supports both the establishment and termination of connections using SYN and FIN packets, respectively, mimicking the connection-oriented nature of TCP.
* High Transfer Rates: Designed to sustain high data transfer rates, tested up to hundreds of megabits per second, under both low and heavy packet loss scenarios.
* Dynamic Retransmission Timeouts (RTO): Incorporates dynamic RTO adjustment based on measured round-trip times, enhancing the protocol's responsiveness and efficiency in varying network conditions.
* Comprehensive Packet Loss Handling: Features mechanisms to handle packet losses efficiently, including a retransmission strategy that ensures data integrity and delivery.
* Flow Control: Implements flow control to manage the rate of data transmission based on the receiver's capacity, preventing buffer overflow and ensuring smooth data flow.
* Error Detection and Handling: Utilizes custom error handling strategies to manage and recover from transmission errors, ensuring robustness.
* Performance Metrics: Provides detailed statistics on transmission performance, including data rates and efficiency under different network conditions.

## System Architecture
The system is structured around a UDP socket, interfacing directly with the network to send and receive custom-formatted packets. The core components include:
* Sender Socket Class: Manages outgoing data, handling packet creation, timing, and retransmission.
* Receiver Component: Responsible for acknowledging received packets and managing flow control.
* Error and Flow Control Mechanisms: Dynamically adjusts to network conditions to optimize throughput and minimize latency.

## Usage Scenario
This transport layer is ideal for applications requiring high reliability and performance over potentially unreliable network links, such as video streaming, large-scale file transfers, or real-time data feeds in industrial systems.

## Configuration and Deployment
Users can configure various parameters such as buffer sizes, window sizes, and loss probabilities to tailor the service to specific network environments and requirements. The system is deployed as a standalone application with command-line interface for ease of testing and integration.

## Performance and Testing
Extensive testing under various network conditions demonstrates the capability of the transport layer to maintain integrity and performance, providing detailed logs and metrics for analysis. The system's performance can be monitored in real-time, with statistics provided for throughput, packet loss recovery, and other critical indicators.
