package main.java;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

public class PhysiologicalSignalProcessor {
    // Simulated sensor data structure
    public static class SensorData {
        private double time;
        private double emg;
        private double emgFiltered;
        private double heartRate;

        public SensorData(double time, double emg, double heartRate) {
            this.time = time;
            this.emg = emg;
            this.heartRate = heartRate;
            this.emgFiltered = emg; // Initialize filtered as raw
        }

        // Getters
        public double getTime() { return time; }
        public double getEmg() { return emg; }
        public double getEmgFiltered() { return emgFiltered; }
        public double getHeartRate() { return heartRate; }
        public void setEmgFiltered(double emgFiltered) { this.emgFiltered = emgFiltered; }
    }

    private final List<SensorData> dataBuffer;
    private final int windowSize;
    private final AtomicBoolean isRunning;
    private Thread acquisitionThread;

    public PhysiologicalSignalProcessor(int windowSize) {
        this.dataBuffer = new ArrayList<>();
        this.windowSize = windowSize;
        this.isRunning = new AtomicBoolean(false);
    }

    // Start data acquisition
    public void startAcquisition() {
        if (isRunning.compareAndSet(false, true)) {
            acquisitionThread = new Thread(() -> {
                while (isRunning.get()) {
                    SensorData data = simulateSensorData();
                    synchronized (dataBuffer) {
                        dataBuffer.add(data);
                        applyMovingAverageFilter();
                        // Keep last 100 points to prevent buffer overflow
                        if (dataBuffer.size() > 100) {
                            dataBuffer.remove(0);
                        }
                    }
                    try {
                        Thread.sleep(100); // Simulate 10Hz sampling
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                        break;
                    }
                }
            });
            acquisitionThread.start();
        }
    }

    // Stop data acquisition
    public void stopAcquisition() {
        if (isRunning.compareAndSet(true, false) && acquisitionThread != null) {
            try {
                acquisitionThread.join();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }

    // Simulate sensor data
    private SensorData simulateSensorData() {
        double time = System.currentTimeMillis() / 1000.0;
        double emg = Math.sin(time * 0.5) + Math.random() * 0.2; // Simulated EMG
        double heartRate = 60 + Math.sin(time * 0.1) * 10 + Math.random() * 5; // Simulated HR
        return new SensorData(time, emg, heartRate);
    }

    // Apply moving average filter to EMG data
    private void applyMovingAverageFilter() {
        synchronized (dataBuffer) {
            for (int i = Math.max(0, dataBuffer.size() - windowSize); i < dataBuffer.size(); i++) {
                if (i >= windowSize - 1) {
                    double sum = 0;
                    for (int j = i - windowSize + 1; j <= i; j++) {
                        sum += dataBuffer.get(j).getEmg();
                    }
                    double avg = sum / windowSize;
                    dataBuffer.get(i).setEmgFiltered(avg);
                }
            }
        }
    }

    // Get current data buffer
    public List<SensorData> getData() {
        synchronized (dataBuffer) {
            return new ArrayList<>(dataBuffer);
        }
    }

    // Simple API endpoint simulation (e.g., for integration with frontend)
    public String getDataAsJson() {
        StringBuilder json = new StringBuilder("[");
        synchronized (dataBuffer) {
            for (int i = 0; i < dataBuffer.size(); i++) {
                SensorData data = dataBuffer.get(i);
                json.append(String.format(
                    "{\"time\":%.2f,\"emg\":%.2f,\"emgFiltered\":%.2f,\"heartRate\":%.2f}",
                    data.getTime(), data.getEmg(), data.getEmgFiltered(), data.getHeartRate()
                ));
                if (i < dataBuffer.size() - 1) json.append(",");
            }
        }
        json.append("]");
        return json.toString();
    }

    // Main method for testing
    public static void main(String[] args) {
        PhysiologicalSignalProcessor processor = new PhysiologicalSignalProcessor(10);
        processor.startAcquisition();
        try {
            Thread.sleep(2000); // Run for 2 seconds
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        processor.stopAcquisition();
        System.out.println("Collected data: " + processor.getDataAsJson());
    }
}
