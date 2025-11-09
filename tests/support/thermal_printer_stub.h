#ifndef THERMAL_PRINTER_STUB_H
#define THERMAL_PRINTER_STUB_H

#include <cstddef>

class ThermalPrinter {
public:
    enum class PrintJobStage {
        Idle,
        InitSequence,
        LogoStart,
        LogoRow,
        LogoComplete,
        BodyHeader,
        BodyLine,
        Feed,
        Complete
    };

    bool isReady() const { return ready && !errorState; }
    void setReady(bool value) { ready = value; }

    bool printTestPage() {
        printTestPageCalled = true;
        return testPageResult && isReady();
    }

    void setTestPageResult(bool value) { testPageResult = value; }
    void setPrinting(bool value) { printing = value; }
    bool isPrinting() const { return printing; }
    bool hasPendingFortune() const { return jobStage != PrintJobStage::Idle; }

    void setHasError(bool value) { errorState = value; }
    bool hasError() const { return errorState; }

    void resetPrintJob() { jobStage = PrintJobStage::Idle; printing = false; }

    PrintJobStage getJobStage() const { return jobStage; }
    void setJobStage(PrintJobStage stage) { jobStage = stage; }

    size_t queuedFortuneLines() const { return queuedLines; }
    void setQueuedLines(size_t lines) { queuedLines = lines; }

    void cancelCurrentJob() {
        jobStage = PrintJobStage::Idle;
        printing = false;
        cancelCalled = true;
    }

    void clearErrorState() {
        errorState = false;
        clearErrorCalled = true;
    }

    bool printTestPageCalled = false;
    bool cancelCalled = false;
    bool clearErrorCalled = false;

private:
    bool ready = true;
    bool testPageResult = true;
    bool printing = false;
    bool errorState = false;
    PrintJobStage jobStage = PrintJobStage::Idle;
    size_t queuedLines = 0;
};

#endif  // THERMAL_PRINTER_STUB_H
