export function createStateMachine(stateMachineDefinition) {
  var machine = {
    initialState: stateMachineDefinition.initialState,
	states: stateMachineDefinition.states,
    transitionState(GotoState) {
		const currentState = machine.stateStack.pop();
		const nextState = machine.states[GotoState];
		
		currentState.actions.onExit(nextState);
		nextState.actions.onEnter(currentState);
		
		machine.stateStack.push(nextState);
    },
	pushState(SubState) {
		const nextState = machine.states[SubState];	
		nextState.actions.onEnter(currentState);
		machine.stateStack.push(nextState);
    },
	popState() {
		const currentState = machine.stateStack.pop();
		const nextState = machine.stateStack[machine.stateStack.length - 1];
		currentState.actions.onExit(nextState);
    }
  }
  
  machine.stateStack = [];
  machine.stateStack.push( machine.states[machine.initialState] );
  
  return machine;
}
