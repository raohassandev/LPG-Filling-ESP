
# LPG Filling Station System Development Plan

## Phase 1: Minimal Prototype

The first phase will focus on developing a working prototype that includes core features for presentation, with minimal effort. The focus will be on key features to demonstrate the functionality of the LPG filling station system.

### Core Features for the Prototype:
1. **LPG Filling Control**:
   - Use `FillController` to handle the basic logic for starting and stopping the filling process.
   - Use `RelayBank` to control relays for pumps and valves.

2. **Sensor Integration**:
   - Use `InputExpander` to read sensor data (e.g., pressure and weight sensors).
   - Use `WeightService` to monitor the amount of LPG dispensed.
   - Treat operator-entered empty-cylinder tare weight as a business value.
   - Calculate net fill weight as `live weight - tare weight`.

3. **Web Interface**:
   - Use `WebPortal` for controller APIs and a lightweight diagnostics landing page.
   - Use the Expo Android app for the operator, admin, and manufacturer UI.
   - Use `NetworkManager` to handle network connectivity (Wi-Fi).
   - Operator UI must show live, tare, net, target, and amount.
   - Admin UI must manage rate per kg and show sales statistics.

4. **Data Logging**:
   - Use `EventLog` to log critical system events (e.g., start/stop of filling, errors).
   - Use `TransactionLog` to store transaction data for each filling operation.
   - Transaction records must include tare kg, net kg, rate per kg, and final amount.

5. **Modbus/HMI Contract**:
   - Expose fast polling registers for live weight, tare weight, net weight, status, target, and E-stop.
   - Use kg x 100 scaling for 16-bit HMI registers.
   - Bind the register map to Modbus TCP and RTU transport after final HMI/SCADA hardware selection.

5. **Efficient Resource Management**:
   - Optimize memory usage and processing to avoid overloading the board.
   - Use interrupts for sensors where applicable to avoid continuous polling.

### Unit Testing for Prototype:
- **`FillController`**: Test the start/stop of the filling process.
- **`RelayBank`**: Test relay switching functionality.
- **`EventLog`**: Test logging of system events.
- **`TransactionLog`**: Test transaction recording.

## Phase 2: Full System Development

Once the prototype is approved, we will move forward with the full system, including advanced features and cloud integration.

### Advanced Features:
1. **Role-based UI**:
   - Implement role-based access for different users: Operators, Managers, and Owners.
   - Provide different levels of access (e.g., Operators can only start/stop filling, while Managers and Owners have more control).

2. **Cloud Integration**:
   - Integrate with a cloud platform for centralized monitoring and management of multiple stations.
   - Use MQTT for real-time communication between devices and cloud.

3. **Comprehensive Reporting**:
   - Develop detailed reporting features, including:
     - Historical data (e.g., amount dispensed, station performance).
     - Compliance reports (e.g., safety checks).

4. **Error Handling**:
   - Implement robust fault detection (e.g., overflow, sensor failure) and error handling.

5. **Unit Testing and Integration Testing**:
   - Write unit tests for each module (e.g., `FillController`, `RelayBank`).
   - Perform integration testing to ensure modules work together seamlessly.

### Resource Optimization:
- **Use of Interrupts**: To manage input/output (e.g., sensors, relays) efficiently without overloading the board.
- **Memory Management**: Ensure that logs and data storage do not consume excessive memory, especially when the system scales.

## Codex Standard Operating Procedures (SOP)

### Codebase Streamlining:
1. **Modular Programming**:
   - Each module (e.g., `FillController`, `RelayBank`, `EventLog`) should be self-contained and interact only via well-defined interfaces.
   - Reusable functions should be abstracted to avoid code duplication.

2. **Documentation**:
   - **In-line comments** should describe the functionality of each method and class.
   - Include a comprehensive **README** file outlining the system's architecture and setup instructions.

3. **Version Control**:
   - Use **Git Flow** for managing branches and releases.
   - Write clear **commit messages** describing changes (e.g., "Added unit test for FillController").

4. **Testing**:
   - Write unit tests for each core module and integration tests for the complete system.
   - Use frameworks like **Google Test** for C++ or **Arduino Unit Testing** for testing individual components.

### System Setup and Configuration:
1. **Pre-Installation Requirements**:
   - Ensure all hardware (relays, sensors, and controllers) is available and properly connected.
   - Network setup: Ensure Wi-Fi is available for connectivity.

2. **Installation Process**:
   - Physically install the **KC868-A6 controller**, relays, and sensors.
   - Configure the network settings via the web UI.

3. **Testing and Calibration**:
   - Calibrate sensors (e.g., pressure, weight) for accuracy.
   - Run initial tests to ensure that the dispensing process works correctly.

---

## Next Steps

1. **Phase 1: Minimal Prototype**:
   - Begin developing core features like filling control, sensor monitoring, and relay management.
   - Set up the web UI and integrate logging functionality.
   - Write unit tests for the critical components.

2. **Phase 2: Full System Development**:
   - After approval of the prototype, move on to integrating advanced features, cloud support, and a comprehensive UI.

3. **Ongoing Improvements**:
   - Optimize the system as needed based on testing results.
   - Ensure that the system is scalable for larger installations (multiple dispensers, multiple stations).

---

## Conclusion

This plan provides a structured approach to develop the LPG filling station system in two phases: the prototype and the full system. The prototype will demonstrate the key features with minimal effort, while the full system will integrate advanced functionality, including cloud support, role-based UIs, and reporting. The code will be streamlined for efficiency, and testing will ensure the reliability of each module and the system as a whole.
