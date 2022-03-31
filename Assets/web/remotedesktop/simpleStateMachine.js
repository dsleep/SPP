export function createStateMachine(stateMachineDefinition) {
  const machine = {
    currentStateName: stateMachineDefinition.initialState,
	states: stateMachineDefinition.states,
    transition(GotoState) {
		if(GotoState == currentStateName) 
		{
			return;
		}
		const currentState = machine.states[currentStateName];
		const nextState = machine.states[GotoState];

		currentState.actions.onExit(nextState);
		nextState.actions.onEnter(currentState);

		machine.currentStateName = GotoState

		return machine.value
    },
  }
  return machine;
}
