
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q112: How to Communicate Technical Solutions with Designers?

## Problem Analysis

This question tests cross-functional communication skills:
- Requirement understanding
- Solution explanation
- Feasibility assessment
- Collaborative delivery

---

## 1. Communication Principles

### 1.1 Communication Key Points

```
┌─────────────────────────────────────────────────────────────┐
│            Principles for Communicating with Designers       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Understand the requirement:                                │
│  ├── Understand the core fun                                │
│  ├── Clarify target users                                   │
│  ├── Understand the expected effect                         │
│  └── Ask about boundary conditions                          │
│                                                             │
│  Explain constraints:                                       │
│  ├── Technical feasibility                                  │
│  ├── Performance limits                                     │
│  ├── Development cost                                       │
│  └── Maintenance cost                                       │
│                                                             │
│  Provide solutions:                                         │
│  ├── Core requirements first                                │
│  ├── Phased implementation                                  │
│  ├── Alternative solutions                                  │
│  └── Risk explanation                                       │
│                                                             │
│  Communicate continuously:                                  │
│  ├── Regularly sync progress                                │
│  ├── Report issues promptly                                 │
│  ├── Decide and adjust together                             │
│  └── Validate the expected effect                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Requirement Analysis

### 2.1 Requirement Understanding Framework

```python
# Requirement analysis framework

class RequirementAnalysis:
    """Requirement analysis"""

    def analyze(self, requirement):
        """Analyze requirement"""
        return {
            'core_value': self._find_core_value(requirement),
            'target_users': self._identify_target_users(requirement),
            'success_metrics': self._define_success_metrics(requirement),
            'constraints': self._identify_constraints(requirement),
            'risks': self._assess_risks(requirement)
        }

    def _find_core_value(self, requirement):
        """Find core value"""
        questions = [
            "What problem does this feature solve?",
            "What fun do players get?",
            "What benefit does it bring to the game?"
        ]
        # Discuss and answer these questions with the designer
        return "Core value description"

    def _identify_target_users(self, requirement):
        """Identify target users"""
        return {
            'player_type': 'newbie/veteran/paying/free',
            'playstyle': 'casual/hardcore/social/competitive',
            'expected_participation': 'Expected participation'
        }

    def _define_success_metrics(self, requirement):
        """Define success metrics"""
        return {
            'usage': 'Usage rate',
            'retention': 'Retention impact',
            'revenue': 'Revenue impact',
            'engagement': 'Engagement'
        }

# Requirement analysis example
"""
Designer proposes: "Implement cross-server trading"

Requirement analysis:

1. Core value:
   - Enliven the economy
   - Increase socializing
   - Improve retention

2. Target users:
   - Mid-to-high level players
   - Players with trading needs

3. Success metrics:
   - Transaction volume > 1000/day
   - Participating players > 20%
   - Complaints < 1%

4. Constraints:
   - No item duping
   - Performance must not be affected
   - Anti-addiction limits required

5. Risk assessment:
   - Economic system impact
   - Cheat risk
   - Customer support pressure
"""
```

---

## 3. Solution Communication

### 3.1 Solution Presentation

```python
# Solution communication framework

class SolutionPresentation:
    """Solution presentation"""

    def present(self, requirement):
        """Present solution"""
        return {
            'understanding': self._show_understanding(requirement),
            'approach': self._explain_approach(requirement),
            'feasibility': self._assess_feasibility(requirement),
            'timeline': self._estimate_timeline(requirement),
            'tradeoffs': self._discuss_tradeoffs(requirement)
        }

    def _show_understanding(self, requirement):
        """Show understanding"""
        return """
        I understand your requirement as:

        Core goal: {core goal}
        Target users: {target users}
        Expected effect: {expected effect}

        Is my understanding correct?
        """

    def _explain_approach(self, requirement):
        """Explain the solution"""
        return """
        Recommended solution:

        Architecture design:
        - Use a dedicated trading server
        - Message queue for asynchronous processing
        - Redis caching for hot data

        Core flow:
        1. Player lists items
        2. Trading service validates
        3. Database persistence
        4. Broadcast updates

        Security measures:
        - Trade frequency limits
        - Amount validation
        - Logging
        """

    def _assess_feasibility(self, requirement):
        """Assess feasibility"""
        return {
            'technical': 'Technical feasibility: High',
            'performance': 'Performance impact: Low',
            'security': 'Security risk: Medium, needs stronger validation',
            'maintenance': 'Maintenance cost: Medium'
        }

    def _estimate_timeline(self, requirement):
        """Estimate timeline"""
        return """
        Timeline estimate:

        Phase 1 (foundation): 2 weeks
        - Architecture setup
        - Basic functionality
        - Internal testing

        Phase 2 (features): 2 weeks
        - Trading features
        - Search features
        - Integration testing

        Phase 3 (optimization): 1 week
        - Performance optimization
        - Security hardening
        - Launch preparation

        Total: 5 weeks
        """

    def _discuss_tradeoffs(self, requirement):
        """Discuss tradeoffs"""
        return """
        Solution options:

        Option A: Simple and fast
        Pros: Fast development, low cost
        Cons: Limited features, poor extensibility

        Option B: Complete solution (recommended)
        Pros: Complete features, extensible
        Cons: Longer development time, higher cost

        Suggestion:
        - Implement the core of Option B first
        - Roll out in stages and validate the effect
        - Decide follow-up investment based on data
        """
```

---

## 4. Common Scenarios

### 4.1 Scenario Handling

```python
# Common communication scenarios

class CommonScenarios:
    """Common scenarios"""

    scenarios = {

        'Feasibility_Question': {
            'scenario': 'Designer asks "Can this be implemented?"',
            'answer_framework': '''
            1. Technically it can be implemented
            2. But consider:
               - Development cost: X weeks
               - Performance impact: needs optimization
               - Maintenance cost: needs a dedicated owner
            3. Suggest implementing the core feature first and validating the effect
            '''
        },

        'Change_Request': {
            'scenario': 'Requirement changes mid-development',
            'handling': '''
            1. Understand the reason for the change
            2. Assess the impact:
               - Workload required for the change
               - Impact on the schedule
               - Impact on other features
            3. Provide options:
               - Accept all (delayed)
               - Accept partially (phased)
               - Reject (explain why)
            '''
        },

        'Performance_Concern': {
            'scenario': 'Designer worries about performance',
            'explanation': '''
            1. Show test data
            2. Explain the optimization plan:
               - Partitioned processing
               - Cache optimization
               - Asynchronous processing
            3. Provide monitoring
            4. Define a scaling plan
            '''
        },

        'Timeline_Pressure': {
            'scenario': 'Time is tight',
            'handling': '''
            1. Sort out priorities:
               - Core features must be done
               - Auxiliary features can be simplified
               - Nice-to-haves can be deferred
            2. Parallel development
            3. Reuse existing solutions
            4. Modest overtime (short term)
            '''
        }
    }
```

---

## 5. Communication Skills

### 5.1 Effective Communication

```python
# Communication skills

class CommunicationSkills:
    """Communication skills"""

    tips = {
        'Use_Analogy': {
            'tip': 'Use analogy',
            'example': '''
            Designer: "Why can't we synchronise all player positions in real time?"
            Answer: "Imagine 1000 people talking in the same room at once —
                   could you make out what everyone is saying? It's the same for us;
                   players only need to see the people nearby."
            '''
        },

        'Show_Not_Tell': {
            'tip': 'Show rather than tell',
            'example': 'Demonstrate a prototype/effect directly instead of just describing it'
        },

        'Provide_Options': {
            'tip': 'Provide options',
            'example': '''
            "We have three options:
             A: Fast but few features
             B: Slow but complete features
             C: A compromise
             Which do you prefer?"
            '''
        },

        'Focus_Value': {
            'tip': 'Focus on value',
            'example': '''
            "This implementation is complex, but it delivers a better player experience,
             and it is worth it in the long run."
            '''
        }
    }
```

---

## 6. Collaboration Process

### 6.1 Standard Process

```
Requirement review → Technical solution → Review confirmation → Development → Acceptance testing → Launch and operation

Requirement review:
- Designer explains the requirement
- Developers ask clarifying questions
- Confirm the understanding together

Technical solution:
- Developers design the solution
- Designer confirms the effect
- Assess development cost

Development:
- Regularly sync progress
- Report issues promptly
- Coordinate resources

Acceptance testing:
- Designer accepts the features
- Feed back the issue list
- Developers fix them

Launch and operation:
- Monitor data performance
- Collect player feedback
- Plan follow-up iterations
```

---

## 7. Best Practices

### 7.1 Collaboration Advice

| Practice | Description |
|----------|-------------|
| **See it their way** | Understand the designer's difficulties |
| **Let data speak** | Support decisions with data |
| **Prototype first** | Validate ideas quickly |
| **Keep docs in sync** | Keep information consistent |
| **Timely feedback** | Don't wait until the end to speak up |
| **Build trust** | Keep your promises |

---

## 8. Summary

### Core of Communicating with Designers

```
Communicating with designers = understand requirements + explain constraints + provide solutions + collaborate continuously
- Understand requirements from their perspective
- Translate technical language into business language
- Offer several solution options
- Keep communication open
```

---

## References

- [Product Management for Developers](https://www.amazon.com/Inspired-How-Create-Tech-Products-Inspired/dp/1119387507)
- [Technical Communication](https://www.amazon.com/Engineers-Engineers-Guide-Communication-Software/dp/1544946741)
