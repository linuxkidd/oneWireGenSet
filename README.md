# oneWireGenSet
Start/Stop two wire control Generators from one wire input

# Overview
- Generators like the ONAN Quiet Diesel line use a separate 'start' and 'stop' signal wire.
- The signal wire must remain active until the desired state is reached.
- However, many (all?) Victron gear, like the Cerbo GX, only provide a single Generator relay output.
- When the relay contacts are closed, the GenSet is expected to start and remain running.
- When the relay contacts are open, the GenSet is expected to stop and remain stopped ( unless manually started via another mechanism ).

# Operation Logic:

## GenSet Start:
1. Send a 'Stop' signal for 10 seconds to prime fuel to the GenSet.
2. Wait for 4 seconds.
3. Send a 'Start' signal for 20 seconds or until GenSet Running input active.
4. If the GenSet is -not- running, wait for 2 minutes and retry up to 3 total times.
5. If the GenSet starts, but does not remain running for 20 seconds, fall back to step 4.

## GenSet Stop:
1. Send a 'Stop' signal for 10 seconds

- The Start timing and steps can be adjusted if desired via the `START_STEP_MILLIS`, and `START_STEPS` constants toward the top of the code.
- The Stop timing can be adjusted using the `STOP_MILLIS` constant toward the top of the code.

**WARNING:**
- There are references to the Start step numbers in other parts of the code.
- Changes to the step count or placement will require further adjustments.
